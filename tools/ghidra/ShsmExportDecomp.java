// Decompiles selected functions into one text file each: reading aids for decomp.exe, never source.
//
// Headless post-script, run by decomp.exe; not meant to be run by hand.
//
//   analyzeHeadless <projectDir> shsm -process <program> -noanalysis -readOnly ^
//       -scriptPath <repo>\tools\ghidra ^
//       -postScript ShsmExportDecomp.java <outDir> <addresses.txt> <results.tsv> <workers> <timeoutSecs> <stamp>
//
// Arguments:
//   outDir         where the listings go: <outDir>/<ADDR>_<name>.c
//   addresses.txt  one hex function address per line ("all" instead of a path: every function)
//   results.tsv    written at the end: address, name, file, status (ok/failed), reason, refs
//                  (refs: comma-separated hex addresses the function's instructions reference, calls and data)
//   workers        decompiler threads, one native decompiler process each
//   timeoutSecs    per-function decompiler timeout
//   stamp          the symbols.txt stamp the program's names were applied from (written into each header)
//
// The decompiler instances are opened once per worker thread (the parallel pattern of that project's
// ExportDecomp.java). A function that fails gets "/* DECOMPILE FAILED: <reason> */" and status "failed";
// it never stops the export. Prints "EXPORT DONE: <n> functions, <f> failed, <s>s" at the end.
//
// @category SHSM

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.decompiler.DecompiledFunction;
import ghidra.app.script.GhidraScript;
import ghidra.framework.Application;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.util.task.TaskMonitor;

public class ShsmExportDecomp extends GhidraScript {

	private int timeoutSecs = 60;
	private final ConcurrentLinkedQueue<DecompInterface> allDecompilers = new ConcurrentLinkedQueue<>();
	private final ThreadLocal<DecompInterface> localDecompiler = new ThreadLocal<>();
	private String header;

	private static final class Out {
		String address;
		String name;
		String file = "";
		boolean ok;
		String reason = "";
		String refs = "";
	}

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 6) {
			printerr("EXPORT FAILED: usage: ShsmExportDecomp.java <outDir> <addresses|all> <results.tsv> " +
				"<workers> <timeoutSecs> <stamp>");
			return;
		}
		Path out = Paths.get(args[0]).toAbsolutePath();
		Files.createDirectories(out);
		int workers = Math.max(1, Integer.parseInt(args[3]));
		timeoutSecs = Integer.parseInt(args[4]);
		header = "// ghidra: " + Application.getApplicationVersion() + "  language: " +
			currentProgram.getLanguageID() + "  symbols.txt: " + args[5] + "\n";

		List<Function> funcs = new ArrayList<>();
		List<Out> missing = new ArrayList<>();
		if (args[1].equals("all")) {
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				if (f != null && !f.isExternal()) {
					funcs.add(f);
				}
			}
		}
		else {
			AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
			for (String line : Files.readAllLines(Paths.get(args[1]), StandardCharsets.UTF_8)) {
				line = line.trim();
				if (line.isEmpty()) {
					continue;
				}
				Address a = space.getAddress(Long.parseLong(line, 16));
				Function f = currentProgram.getFunctionManager().getFunctionAt(a);
				if (f == null) {
					Out o = new Out();
					o.address = String.format("%08X", a.getOffset());
					o.name = "";
					o.reason = "no function at this address in the Ghidra project";
					missing.add(o);
				}
				else {
					funcs.add(f);
				}
			}
		}
		println(funcs.size() + " functions to decompile (" + workers + " workers, " + timeoutSecs + "s timeout)");
		long t0 = System.currentTimeMillis();
		List<Out> results = new ArrayList<>(missing);
		if (!funcs.isEmpty()) {
			results.addAll(decompileAll(out, funcs, workers));
		}
		int failed = 0;
		Path tmp = Paths.get(args[2] + ".tmp");
		try (BufferedWriter w = Files.newBufferedWriter(tmp, StandardCharsets.UTF_8)) {
			w.write("address\tname\tfile\tstatus\treason\trefs\n");
			for (Out o : results) {
				if (!o.ok) {
					failed++;
				}
				w.write(o.address + "\t" + o.name + "\t" + o.file + "\t" + (o.ok ? "ok" : "failed") + "\t" +
					clean(o.reason) + "\t" + o.refs + "\n");
			}
		}
		Files.move(tmp, Paths.get(args[2]), StandardCopyOption.REPLACE_EXISTING);
		println("EXPORT DONE: " + results.size() + " functions, " + failed + " failed, " +
			(System.currentTimeMillis() - t0) / 1000 + "s");
	}

	private List<Out> decompileAll(Path out, List<Function> funcs, int workers) throws Exception {
		ExecutorService pool = Executors.newFixedThreadPool(workers, r -> {
			Thread t = new Thread(r, "ShsmExportDecomp-worker");
			t.setDaemon(true);
			return t;
		});
		List<Out> results = new ArrayList<>(funcs.size());
		AtomicInteger done = new AtomicInteger();
		long t0 = System.currentTimeMillis();
		try {
			List<Future<Out>> futures = new ArrayList<>(funcs.size());
			for (Function f : funcs) {
				futures.add(pool.submit(() -> {
					Out o = exportOne(out, f);
					int n = done.incrementAndGet();
					if (n % 500 == 0) {
						println(n + "/" + funcs.size() + " decompiled, " + (System.currentTimeMillis() - t0) / 1000 +
							"s");
					}
					return o;
				}));
			}
			for (Future<Out> fu : futures) {
				results.add(fu.get());
			}
		}
		finally {
			pool.shutdownNow();
			pool.awaitTermination(30, TimeUnit.SECONDS);
			for (DecompInterface d : allDecompilers) {
				try {
					d.dispose();
				}
				catch (Throwable t) {
					// nothing to do at the end
				}
			}
			allDecompilers.clear();
		}
		return results;
	}

	private Out exportOne(Path out, Function f) {
		Out o = new Out();
		o.address = String.format("%08X", f.getEntryPoint().getOffset());
		o.name = clean(f.getName());
		String code;
		try {
			o.refs = refs(f);
			code = decompile(f, o);
		}
		catch (Throwable t) {
			o.ok = false;
			o.reason = String.valueOf(t);
			code = null;
		}
		if (code == null) {
			code = "/* DECOMPILE FAILED: " + clean(o.reason).replace("*/", "* /") + " */\n";
		}
		String plate = f.getComment();
		StringBuilder sb = new StringBuilder(code.length() + 512);
		sb.append("// symbol: ").append(o.name).append('\n');
		if (plate != null && !plate.isEmpty()) {
			sb.append("// demangled: ").append(clean(plate)).append('\n');
		}
		sb.append(String.format("// address: 0x%s  size: 0x%X%n", o.address, f.getBody().getNumAddresses()));
		sb.append(header).append('\n');
		sb.append(code);
		o.file = o.address + "_" + fileSafe(o.name) + ".c";
		try {
			Path p = out.resolve(o.file);
			Path tmp = out.resolve(o.file + ".tmp");
			Files.write(tmp, sb.toString().getBytes(StandardCharsets.UTF_8));
			Files.move(tmp, p, StandardCopyOption.REPLACE_EXISTING);
		}
		catch (Exception e) {
			o.ok = false;
			o.reason = "write: " + e;
			o.file = "";
		}
		return o;
	}

	private String decompile(Function f, Out o) {
		DecompInterface ifc = decompiler();
		DecompileResults res = ifc.decompileFunction(f, timeoutSecs, TaskMonitor.DUMMY);
		if (res != null && res.decompileCompleted()) {
			DecompiledFunction df = res.getDecompiledFunction();
			if (df != null && df.getC() != null && !df.getC().trim().isEmpty()) {
				o.ok = true;
				return df.getC().replace("\r\n", "\n").replaceAll("^\n+", "");
			}
			o.reason = "no decompiled code";
			return null;
		}
		if (res == null) {
			o.reason = "no result";
		}
		else if (res.failedToStart()) {
			o.reason = "the decompiler process did not start";
		}
		else if (res.isTimedOut()) {
			o.reason = "timed out after " + timeoutSecs + "s";
		}
		else {
			String m = res.getErrorMessage();
			o.reason = m == null || m.trim().isEmpty() ? "no decompiled code" : m;
		}
		// A decompiler that timed out or crashed is replaced for the next function on this thread.
		localDecompiler.remove();
		allDecompilers.remove(ifc);
		ifc.dispose();
		return null;
	}

	private String refs(Function f) {
		Set<String> seen = new LinkedHashSet<>();
		InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
		while (it.hasNext()) {
			Instruction ins = it.next();
			for (Reference r : ins.getReferencesFrom()) {
				if (r.isMemoryReference() && !r.getReferenceType().isFlow() || r.getReferenceType().isCall()) {
					seen.add(String.format("%08X", r.getToAddress().getOffset()));
				}
			}
		}
		return String.join(",", seen);
	}

	private DecompInterface decompiler() {
		DecompInterface d = localDecompiler.get();
		if (d == null) {
			d = new DecompInterface();
			DecompileOptions options = new DecompileOptions();
			options.grabFromProgram(currentProgram);
			d.setOptions(options);
			d.toggleCCode(true);
			d.toggleSyntaxTree(true);
			d.setSimplificationStyle("decompile");
			if (!d.openProgram(currentProgram)) {
				String m = d.getLastMessage();
				d.dispose();
				throw new IllegalStateException("could not open the program in the decompiler: " + m);
			}
			allDecompilers.add(d);
			localDecompiler.set(d);
		}
		return d;
	}

	private static String clean(String s) {
		return s == null ? "" : s.replaceAll("[\\t\\r\\n]+", " ").trim();
	}

	/** File-name spelling of a symbol: Windows-invalid characters become '_', at most 80 characters. */
	static String fileSafe(String name) {
		String s = name.replaceAll("[<>:\"/\\\\|?*\\s]", "_");
		return s.length() > 80 ? s.substring(0, 80) : s;
	}
}
