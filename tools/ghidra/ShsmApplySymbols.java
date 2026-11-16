// Applies the project's symbols (config/<ver>/symbols.txt, converted by decomp.exe) to the program.
//
// Headless script, run by decomp.exe; not meant to be run by hand.
//
//   analyzeHeadless <projectDir> shsm -import main.elf -processor PowerPC:BE:32:Gekko_Broadway ^
//       -scriptPath <repo>\tools\ghidra ^
//       -preScript ShsmApplySymbols.java pre <symbols.tsv> <SDA_BASE_> <SDA2_BASE_> ^
//       -postScript ShsmApplySymbols.java post <symbols.tsv>
//   analyzeHeadless <projectDir> shsm -process <program> -noanalysis ^
//       -postScript ShsmApplySymbols.java refresh <delta.tsv> <SDA_BASE_> <SDA2_BASE_>
//
// Modes:
//   pre      before auto-analysis: make the constant blocks (.rodata, .sdata2) read-only so the decompiler
//            folds their constants, disassemble every function body, then apply.
//   (any mode given the two bases: set r13/r2 to the small-data bases and GQR0 to 0 over every executable
//            block, so r13/r2-relative accesses resolve to addresses and qr0 scaling folds away.)
//   post     after auto-analysis: apply again (analysis may have moved bodies or names).
//   refresh  apply the rows of a delta file only (rows of kind R remove a symbol decomp.exe no longer has).
//
// Input: UTF-8 TSV, one symbol per line, columns
//   kind     F function, O object, L label, R removed (refresh only)
//   address  hex, no prefix
//   size     decimal bytes (0 = unknown)
//   name     the raw symbols.txt name (the label: listings map back to symbols.txt by it)
//   demangled  demangled text, or empty (functions: written as the plate comment)
//   dtype    objects: float, double, string or empty (the data type to lay down when the size fits)
//   this     functions: the class of the implicit this parameter ("A::B"), or empty
//   params   functions: "-" = no signature known; else ";"-separated parameter types, each a base
//            (void bool char uchar short ushort int uint long ulong longlong ulonglong float double
//            wchar code, or S:<A::B> a class) followed by one "*" per pointer level; "..." = varargs;
//            "" = no parameters.
//
// Prints "APPLY DONE: <counts>" at the end; failures to apply single symbols are counted, never fatal.
//
// @category SHSM

import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.BooleanDataType;
import ghidra.program.model.data.CategoryPath;
import ghidra.program.model.data.CharDataType;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeConflictHandler;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.DoubleDataType;
import ghidra.program.model.data.FloatDataType;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.data.LongDataType;
import ghidra.program.model.data.LongLongDataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.ShortDataType;
import ghidra.program.model.data.StringDataType;
import ghidra.program.model.data.StructureDataType;
import ghidra.program.model.data.UnsignedCharDataType;
import ghidra.program.model.data.UnsignedIntegerDataType;
import ghidra.program.model.data.UnsignedLongDataType;
import ghidra.program.model.data.UnsignedLongLongDataType;
import ghidra.program.model.data.UnsignedShortDataType;
import ghidra.program.model.data.Undefined1DataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.data.WideCharDataType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SymbolType;
import ghidra.program.model.symbol.SymbolUtilities;

public class ShsmApplySymbols extends GhidraScript {

	private static final class Row {
		char kind;
		Address addr;
		long size;
		String name;
		String demangled;
		String dtype;
		String thisClass;
		String params;
	}

	private DataTypeManager dtm;
	private final Map<String, DataType> classTypes = new HashMap<>();
	private int functions, created, renamed, bodies, signatures, labels, datas, removed, failures;

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 2) {
			printerr("APPLY FAILED: usage: ShsmApplySymbols.java pre|post|refresh <symbols.tsv> [sda sda2]");
			return;
		}
		String mode = args[0];
		List<Row> rows = read(args[1]);
		dtm = currentProgram.getDataTypeManager();
		long t0 = System.currentTimeMillis();
		if (args.length >= 4) {
			setSmallDataBases(Long.parseLong(args[2], 16), Long.parseLong(args[3], 16));
		}
		if (mode.equals("pre")) {
			readOnlyConstants();
			removeGapLabels();
			disassemble(rows);
		}
		for (Row r : rows) {
			if (monitor.isCancelled()) {
				break;
			}
			try {
				switch (r.kind) {
					case 'F':
						try {
							applyFunction(r);
						}
						catch (java.util.ConcurrentModificationException e) {
							// A function record removed earlier in this run (an overlapping fragment) can
							// still be handed out once; fetched again, the address is free.
							functions--;
							applyFunction(r);
						}
						break;
					case 'O':
					case 'L':
						applyLabel(r);
						break;
					case 'R':
						remove(r);
						break;
					default:
						break;
				}
			}
			catch (Exception e) {
				failures++;
				if (failures <= 20) {
					printerr("apply " + r.name + " @ " + r.addr + ": " + e + " at " +
						(e.getStackTrace().length > 0 ? e.getStackTrace()[0] : "?") + " / " +
						(e.getStackTrace().length > 1 ? e.getStackTrace()[1] : "?"));
				}
			}
		}
		println("APPLY DONE: mode " + mode + ", " + rows.size() + " rows, " + functions + " functions (" + created +
			" created, " + renamed + " renamed, " + bodies + " bodies set, " + signatures + " signatures), " + labels +
			" labels, " + datas + " typed data, " + removed + " removed, " + failures + " failures, " +
			(System.currentTimeMillis() - t0) / 1000 + "s");
	}

	// ---------------------------------------------------------------------------------------------
	// Input
	// ---------------------------------------------------------------------------------------------

	private List<Row> read(String path) throws Exception {
		AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
		List<Row> rows = new ArrayList<>();
		for (String line : Files.readAllLines(Paths.get(path), StandardCharsets.UTF_8)) {
			if (line.isEmpty() || line.startsWith("#")) {
				continue;
			}
			String[] c = line.split("\t", -1);
			if (c.length < 8) {
				continue;
			}
			Row r = new Row();
			r.kind = c[0].charAt(0);
			r.addr = space.getAddress(Long.parseLong(c[1], 16));
			r.size = Long.parseLong(c[2]);
			r.name = c[3];
			r.demangled = c[4];
			r.dtype = c[5];
			r.thisClass = c[6];
			r.params = c[7];
			rows.add(r);
		}
		return rows;
	}

	// ---------------------------------------------------------------------------------------------
	// Program setup (pre mode)
	// ---------------------------------------------------------------------------------------------

	private void setSmallDataBases(long sda, long sda2) throws Exception {
		Register r13 = currentProgram.getRegister("r13");
		Register r2 = currentProgram.getRegister("r2");
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (!b.isExecute()) {
				continue;
			}
			currentProgram.getProgramContext().setValue(r13, b.getStart(), b.getEnd(), BigInteger.valueOf(sda));
			currentProgram.getProgramContext().setValue(r2, b.getStart(), b.getEnd(), BigInteger.valueOf(sda2));
		}
		// GQR0 is 0 for the whole program (the SDK never reprograms it). Known, it folds away the scale
		// pseudo-code the Gekko language models for every psq_l/psq_st ..., qr0 (the FPR saves of prologues).
		Register gqr0 = currentProgram.getRegister("GQR0");
		if (gqr0 != null) {
			for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
				if (b.isExecute()) {
					currentProgram.getProgramContext().setValue(gqr0, b.getStart(), b.getEnd(), BigInteger.ZERO);
				}
			}
		}
		println(String.format("r13 = %08x, r2 = %08x%s over the executable blocks", sda, sda2,
			gqr0 != null ? ", GQR0 = 0" : ""));
	}

	private void readOnlyConstants() {
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			String n = b.getName();
			if ((n.equals(".rodata") || n.equals(".sdata2")) && b.isInitialized()) {
				b.setWrite(false);
				println(n + " made read-only (its constants fold into the listings)");
			}
		}
	}

	/** dtk's linker-gap filler symbols (gap_NN_<addr>_<section>) are not names anyone wrote. */
	private void removeGapLabels() {
		SymbolTable st = currentProgram.getSymbolTable();
		List<Symbol> gaps = new ArrayList<>();
		for (Symbol s : st.getAllSymbols(false)) {
			if (s.getName().startsWith("gap_") && s.getSymbolType() == SymbolType.LABEL) {
				gaps.add(s);
			}
		}
		for (Symbol s : gaps) {
			s.delete();
		}
		println(gaps.size() + " gap_ labels removed");
	}

	private void disassemble(List<Row> rows) {
		AddressSet code = new AddressSet();
		for (Row r : rows) {
			if (r.kind == 'F' && r.size > 0) {
				code.add(r.addr, r.addr.add(r.size - 1));
			}
		}
		DisassembleCommand cmd = new DisassembleCommand(code, code, true);
		cmd.applyTo(currentProgram, monitor);
		println("disassembled " + code.getNumAddresses() + " bytes of function bodies");
	}

	// ---------------------------------------------------------------------------------------------
	// Functions
	// ---------------------------------------------------------------------------------------------

	private void applyFunction(Row r) throws Exception {
		functions++;
		FunctionManager fm = currentProgram.getFunctionManager();
		String name = SymbolUtilities.replaceInvalidChars(r.name, true);
		AddressSet body = r.size > 0 ? new AddressSet(r.addr, r.addr.add(r.size - 1)) : null;
		Function f = fm.getFunctionAt(r.addr);
		if (f == null) {
			if (body != null) {
				// Functions analysis or the ELF loader put inside this body are fragments of it.
				for (Function g : overlapping(fm, body)) {
					if (!g.getEntryPoint().equals(r.addr)) {
						fm.removeFunction(g.getEntryPoint());
					}
				}
			}
			if (getInstructionAt(r.addr) == null) {
				disassemble(r.addr);
			}
			f = fm.createFunction(name, r.addr, body != null ? body : new AddressSet(r.addr), SourceType.IMPORTED);
			created++;
		}
		else {
			if (!f.getName().equals(name)) {
				f.setName(name, SourceType.IMPORTED);
				renamed++;
			}
			else if (f.getSymbol().getSource() != SourceType.IMPORTED) {
				f.setName(name, SourceType.IMPORTED);
			}
			if (body != null && !f.getBody().equals(body)) {
				for (Function g : overlapping(fm, body)) {
					if (!g.getEntryPoint().equals(r.addr)) {
						fm.removeFunction(g.getEntryPoint());
					}
				}
				f.setBody(body);
				bodies++;
			}
		}
		if (!r.demangled.isEmpty()) {
			setPlateComment(r.addr, r.demangled);
		}
		if (!r.params.equals("-")) {
			applySignature(f, r);
		}
	}

	private static List<Function> overlapping(FunctionManager fm, AddressSet body) {
		List<Function> out = new ArrayList<>();
		java.util.Iterator<Function> it = fm.getFunctionsOverlapping(body);
		while (it.hasNext()) {
			out.add(it.next());
		}
		return out;
	}

	private void applySignature(Function f, Row r) throws Exception {
		List<Parameter> params = new ArrayList<>();
		boolean varargs = false;
		if (!r.thisClass.isEmpty()) {
			params.add(new ParameterImpl("this", new PointerDataType(classType(r.thisClass), dtm), currentProgram));
		}
		if (!r.params.isEmpty()) {
			int n = 1;
			for (String p : r.params.split(";")) {
				if (p.equals("...")) {
					varargs = true;
					continue;
				}
				DataType t = type(p);
				if (t == null) {
					return; // a type this script cannot spell: leave the prototype to the decompiler
				}
				params.add(new ParameterImpl("param_" + n++, t, currentProgram));
			}
		}
		f.replaceParameters(params, FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.IMPORTED);
		f.setVarArgs(varargs);
		signatures++;
	}

	private DataType type(String spec) {
		int stars = 0;
		String base = spec;
		while (base.endsWith("*")) {
			stars++;
			base = base.substring(0, base.length() - 1);
		}
		DataType t;
		if (base.startsWith("S:")) {
			t = classType(base.substring(2));
		}
		else {
			t = builtin(base);
		}
		if (t == null) {
			return null;
		}
		for (int i = 0; i < stars; i++) {
			t = new PointerDataType(t, dtm);
		}
		return t;
	}

	private static DataType builtin(String b) {
		switch (b) {
			case "void": return VoidDataType.dataType;
			case "bool": return BooleanDataType.dataType;
			case "char": return CharDataType.dataType;
			case "uchar": return UnsignedCharDataType.dataType;
			case "short": return ShortDataType.dataType;
			case "ushort": return UnsignedShortDataType.dataType;
			case "int": return IntegerDataType.dataType;
			case "uint": return UnsignedIntegerDataType.dataType;
			case "long": return LongDataType.dataType;
			case "ulong": return UnsignedLongDataType.dataType;
			case "longlong": return LongLongDataType.dataType;
			case "ulonglong": return UnsignedLongLongDataType.dataType;
			case "float": return FloatDataType.dataType;
			case "double": return DoubleDataType.dataType;
			case "wchar": return WideCharDataType.dataType;
			case "code": return VoidDataType.dataType; // a function pointer: void* is enough to read
			default: return null;
		}
	}

	/** A placeholder structure per class, in a category per namespace (/shsm/FMOD/SystemI). */
	private DataType classType(String qualified) {
		DataType t = classTypes.get(qualified);
		if (t != null) {
			return t;
		}
		String[] parts = qualified.split("::");
		CategoryPath cat = new CategoryPath("/shsm");
		for (int i = 0; i < parts.length - 1; i++) {
			cat = new CategoryPath(cat, sanitize(parts[i]));
		}
		StructureDataType s = new StructureDataType(cat, sanitize(parts[parts.length - 1]), 0, dtm);
		t = dtm.addDataType(s, DataTypeConflictHandler.KEEP_HANDLER);
		classTypes.put(qualified, t);
		return t;
	}

	private static String sanitize(String s) {
		return s.replaceAll("[\\s/]", "_");
	}

	// ---------------------------------------------------------------------------------------------
	// Data and labels
	// ---------------------------------------------------------------------------------------------

	private void applyLabel(Row r) throws Exception {
		String name = SymbolUtilities.replaceInvalidChars(r.name, true);
		SymbolTable st = currentProgram.getSymbolTable();
		Symbol keep = null;
		for (Symbol s : st.getSymbols(r.addr)) {
			if (s.getName().equals(name) && s.getSymbolType() == SymbolType.LABEL) {
				keep = s;
			}
		}
		if (keep == null) {
			keep = st.createLabel(r.addr, name, SourceType.IMPORTED);
		}
		if (!keep.isPrimary()) {
			keep.setPrimary();
		}
		// Older names of this address (a rename in symbols.txt) go away; Ghidra's own and function symbols stay.
		for (Symbol s : st.getSymbols(r.addr)) {
			if (s != keep && s.getSymbolType() == SymbolType.LABEL && s.getSource() == SourceType.IMPORTED) {
				s.delete();
			}
		}
		labels++;
		// Every sized object gets a data item of its size: an access inside it then names the object
		// (lbl_80491B38._20_4_) instead of a free-standing DAT_80491b4c. Typed where symbols.txt says how.
		if (r.kind == 'O' && r.size > 0 && !inCode(r.addr, r.size)) {
			DataType t = null;
			if (r.dtype.equals("float") && r.size == 4) {
				t = FloatDataType.dataType;
			}
			else if (r.dtype.equals("double") && r.size == 8) {
				t = DoubleDataType.dataType;
			}
			else if (r.dtype.equals("string") && r.size > 0) {
				t = new StringDataType();
			}
			if (t == null && r.size > 1 && onlyUndefined(r.addr, r.size)) {
				// Only over undefined items: data analysis already typed (pointers, vtables) stays.
				t = new ArrayDataType(Undefined1DataType.dataType, (int) r.size, 1);
			}
			boolean initialized = currentProgram.getMemory().getBlock(r.addr) != null &&
				currentProgram.getMemory().getBlock(r.addr).isInitialized();
			if (t != null && (initialized || t instanceof ArrayDataType)) {
				if (r.dtype.equals("string")) {
					clearListing(r.addr, r.addr.add(r.size - 1));
					currentProgram.getListing().createData(r.addr, t, (int) r.size);
				}
				else {
					clearListing(r.addr, r.addr.add(r.size - 1));
					currentProgram.getListing().createData(r.addr, t);
				}
				datas++;
			}
		}
	}

	private boolean onlyUndefined(Address a, long size) {
		ghidra.program.model.listing.DataIterator it =
			currentProgram.getListing().getDefinedData(new AddressSet(a, a.add(size - 1)), true);
		while (it.hasNext()) {
			ghidra.program.model.listing.Data d = it.next();
			if (d.getDataType() instanceof ArrayDataType && d.getAddress().equals(a) && d.getLength() == size) {
				return false; // already laid down
			}
			if (!ghidra.program.model.data.Undefined.isUndefined(d.getDataType())) {
				return false;
			}
		}
		return true;
	}

	private boolean inCode(Address a, long size) {
		MemoryBlock b = currentProgram.getMemory().getBlock(a);
		return b == null || b.isExecute() ||
			currentProgram.getListing().getInstructions(new AddressSet(a, a.add(size - 1)), true).hasNext();
	}

	private void remove(Row r) throws Exception {
		String name = SymbolUtilities.replaceInvalidChars(r.name, true);
		Function f = currentProgram.getFunctionManager().getFunctionAt(r.addr);
		if (f != null && f.getName().equals(name)) {
			currentProgram.getFunctionManager().removeFunction(r.addr);
			removed++;
			return;
		}
		for (Symbol s : currentProgram.getSymbolTable().getSymbols(r.addr)) {
			if (s.getName().equals(name) && s.getSymbolType() == SymbolType.LABEL) {
				s.delete();
				removed++;
			}
		}
	}
}
