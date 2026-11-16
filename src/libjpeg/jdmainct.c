/*
 * jdmainct.c
 *
 * IJG JPEG library 6b: main buffer controller for decompression.
 *
 * This file contains the main buffer controller for decompression.
 * The main buffer lies between the JPEG decompressor proper and the
 * post-processor; it holds downsampled data in the JPEG colorspace.
 *
 * Note that this code is bypassed in raw-data mode, since the application
 * supplies the equivalent of the main buffer in that case.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"

/*
 * In the current system design, the main buffer need never be a full-image
 * buffer; any full-height buffers will be found inside the coefficient or
 * postprocessing controllers.  Nonetheless, the main controller is not
 * trivial.  Its responsibility is to provide context rows for upsampling/
 * rescaling, and doing this in an efficient fashion is a bit tricky.
 *
 * The "wraparound pointers" trick keeps the context rows contiguous in
 * the logical view while only allocating them once.  See the IJG sources
 * for the full description.
 */

/* Private buffer controller object */

typedef struct {
  struct jpeg_d_main_controller pub; /* public fields */

  /* Pointer to allocated workspace (M or M+2 row groups). */
  JSAMPARRAY buffer[MAX_COMPONENTS];

  boolean buffer_full;          /* Have we gotten an iMCU row from decoder? */
  JDIMENSION rowgroup_ctr;      /* counts row groups output to postprocessor */

  /* Remaining fields are only used in the context case. */

  /* These are the master pointers to the funny-order pointer lists. */
  JSAMPIMAGE xbuffer[2];        /* pointers to weird pointer lists */

  int whichptr;                 /* indicates which pointer set is now in use */
  int context_state;            /* process_data state machine status */
  JDIMENSION rowgroups_avail;   /* row groups available to postprocessor */
  JDIMENSION iMCU_row_ctr;      /* counts iMCU rows to detect image top/bot */
} my_main_controller;

typedef my_main_controller * my_main_ptr;

/* context_state values: */
#define CTX_PREPARE_FOR_IMCU    0       /* need to prepare for MCU row */
#define CTX_PROCESS_IMCU        1       /* feeding iMCU to postprocessor */
#define CTX_POSTPONED_ROW       2       /* feeding postponed row group */

/* Forward declarations */
METHODDEF(void) process_data_simple_main(j_decompress_ptr cinfo,
                                         JSAMPARRAY output_buf,
                                         JDIMENSION *out_row_ctr,
                                         JDIMENSION out_rows_avail);
METHODDEF(void) process_data_context_main(j_decompress_ptr cinfo,
                                          JSAMPARRAY output_buf,
                                          JDIMENSION *out_row_ctr,
                                          JDIMENSION out_rows_avail);
#ifdef QUANT_2PASS_SUPPORTED
METHODDEF(void) process_data_crank_post(j_decompress_ptr cinfo,
                                        JSAMPARRAY output_buf,
                                        JDIMENSION *out_row_ctr,
                                        JDIMENSION out_rows_avail);
#endif

LOCAL(void)
alloc_funny_pointers(j_decompress_ptr cinfo)
/* Allocate space for the funny pointer lists.
 * This is done only once, not once per pass.
 */
{
  my_main_ptr main = (my_main_ptr) cinfo->main;
  int ci, rgroup;
  int M = cinfo->min_DCT_scaled_size;
  jpeg_component_info *compptr;
  JSAMPARRAY xbuf;

  /* Get top-level space for component array pointers.
   * We alloc both arrays with one call to save a few cycles.
   */
  main->xbuffer[0] = (JSAMPIMAGE)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                cinfo->num_components * 2 * SIZEOF(JSAMPARRAY));
  main->xbuffer[1] = main->xbuffer[0] + cinfo->num_components;

  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    rgroup = (compptr->v_samp_factor * compptr->DCT_scaled_size) /
      cinfo->min_DCT_scaled_size; /* height of a row group of component */
    /* Get space for pointer lists --- M+4 row groups in each list.
     * We alloc both pointer lists with one call to save a few cycles.
     */
    xbuf = (JSAMPARRAY)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                  2 * (rgroup * (M + 4)) * SIZEOF(JSAMPROW));
    xbuf += rgroup;             /* want one row group at negative offsets */
    main->xbuffer[0][ci] = xbuf;
    xbuf += rgroup * (M + 4);
    main->xbuffer[1][ci] = xbuf;
  }
}

// 0x801BD314
LOCAL(void)
make_funny_pointers(j_decompress_ptr cinfo)
/* Create the funny pointer lists discussed in the comments above.
 * The actual workspace is already allocated (in main->buffer),
 * and the space for the pointer lists is allocated too.
 * This routine just fills in the curiously ordered lists.
 * This will be repeated at the beginning of each pass.
 */
{
  my_main_ptr main = (my_main_ptr) cinfo->main;
  int ci, i, rgroup;
  int M = cinfo->min_DCT_scaled_size;
  jpeg_component_info *compptr;
  JSAMPARRAY buf, xbuf0, xbuf1;

  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    rgroup = (compptr->v_samp_factor * compptr->DCT_scaled_size) /
      cinfo->min_DCT_scaled_size; /* height of a row group of component */
    xbuf0 = main->xbuffer[0][ci];
    xbuf1 = main->xbuffer[1][ci];
    /* First copy the workspace pointers as-is */
    buf = main->buffer[ci];
    for (i = 0; i < rgroup * (M + 2); i++) {
      xbuf0[i] = xbuf1[i] = buf[i];
    }
    /* In the second list, put the last four row groups in swapped order */
    for (i = 0; i < rgroup * 2; i++) {
      xbuf1[rgroup*(M-2) + i] = buf[rgroup*M + i];
      xbuf1[rgroup*M + i] = buf[rgroup*(M-2) + i];
    }
    /* The wraparound pointers at top and bottom will be filled later
     * (see set_wraparound_pointers, below).  Initially we want the "above"
     * pointers to duplicate the first actual data line.  This only needs
     * to happen in xbuffer[0].
     */
    for (i = 0; i < rgroup; i++) {
      xbuf0[i - rgroup] = xbuf0[0];
    }
  }
}

// 0x801BD734
LOCAL(void)
set_wraparound_pointers(j_decompress_ptr cinfo)
/* Set up the "wraparound" pointers at top and bottom of the pointer lists.
 * This changes the pointer list state from top-of-image to the normal state.
 */
{
  my_main_ptr main = (my_main_ptr) cinfo->main;
  int ci, i, rgroup;
  int M = cinfo->min_DCT_scaled_size;
  jpeg_component_info *compptr;
  JSAMPARRAY xbuf0, xbuf1;

  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    rgroup = (compptr->v_samp_factor * compptr->DCT_scaled_size) /
      cinfo->min_DCT_scaled_size; /* height of a row group of component */
    xbuf0 = main->xbuffer[0][ci];
    xbuf1 = main->xbuffer[1][ci];
    for (i = 0; i < rgroup; i++) {
      xbuf0[i - rgroup] = xbuf0[rgroup*(M+1) + i];
      xbuf1[i - rgroup] = xbuf1[rgroup*(M+1) + i];
      xbuf0[rgroup*(M+2) + i] = xbuf0[i];
      xbuf1[rgroup*(M+2) + i] = xbuf1[i];
    }
  }
}

// 0x801BD904
LOCAL(void)
set_bottom_pointers(j_decompress_ptr cinfo)
/* Change the pointer lists to duplicate the last sample row at the bottom
 * of the image.  Whenever we get the last iMCU row, we have to do this.
 */
{
  my_main_ptr main = (my_main_ptr) cinfo->main;
  int ci, i, rgroup, iMCUheight, rows_left;
  jpeg_component_info *compptr;
  JSAMPARRAY xbuf;

  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    /* Count sample rows in one iMCU row and in one row group */
    iMCUheight = compptr->v_samp_factor * compptr->DCT_scaled_size;
    rgroup = iMCUheight / cinfo->min_DCT_scaled_size;
    /* Count nondummy sample rows remaining for this component */
    rows_left = (int) (compptr->downsampled_height % (JDIMENSION) iMCUheight);
    if (rows_left == 0) rows_left = iMCUheight;
    /* Count nondummy row groups.  Longer than necessary, but nothing else. */
    if (ci == 0) {
      main->rowgroups_avail = (JDIMENSION) ((rows_left-1) / rgroup + 1);
    }
    /* Duplicate the last real sample row rgroup*2 times; this pads out the
     * last partial rowgroup and ensures at least one full rowgroup of context.
     */
    xbuf = main->xbuffer[main->whichptr][ci];
    for (i = 0; i < rgroup * 2; i++) {
      xbuf[rows_left + i] = xbuf[rows_left-1];
    }
  }
}

/*
 * Initialize for a processing pass.
 */
// 0x801BDA8C
METHODDEF(void)
start_pass_main(j_decompress_ptr cinfo, J_BUF_MODE pass_mode)
{
  my_main_ptr main = (my_main_ptr) cinfo->main;

  switch (pass_mode) {
  case JBUF_PASS_THRU:
    if (cinfo->upsample->need_context_rows) {
      main->pub.process_data = process_data_context_main;
      make_funny_pointers(cinfo); /* Create the xbuffer[] lists */
      main->whichptr = 0;       /* Read first iMCU row into xbuffer[0] */
      main->context_state = CTX_PREPARE_FOR_IMCU;
      main->iMCU_row_ctr = 0;
    } else {
      /* Simple case with no context needed */
      main->pub.process_data = process_data_simple_main;
    }
    main->buffer_full = FALSE;  /* Mark buffer empty */
    main->rowgroup_ctr = 0;
    break;
#ifdef QUANT_2PASS_SUPPORTED
  case JBUF_CRANK_DEST:
    /* For last pass of 2-pass quantization, just crank the postprocessor */
    main->pub.process_data = process_data_crank_post;
    break;
#endif
  default:
    ERREXIT(cinfo, JERR_BAD_BUFFER_MODE);
    break;
  }
}

/*
 * Process some data.
 * This handles the simple case where no context is required.
 */
// 0x801BDB44
METHODDEF(void)
process_data_simple_main(j_decompress_ptr cinfo,
                         JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
                         JDIMENSION out_rows_avail)
{
  my_main_ptr main = (my_main_ptr) cinfo->main;
  JDIMENSION rowgroups_avail;

  /* Read input data if we haven't filled the main buffer yet */
  if (! main->buffer_full) {
    if (! (*cinfo->coef->decompress_data) (cinfo, main->buffer))
      return;                   /* suspension forced, can do nothing more */
    main->buffer_full = TRUE;   /* OK, we have an iMCU row to work with */
  }

  /* There are always min_DCT_scaled_size row groups in an iMCU row. */
  rowgroups_avail = (JDIMENSION) cinfo->min_DCT_scaled_size;
  /* Note: at the bottom of the image, we may pass extra garbage row groups
   * to the postprocessor.  The postprocessor has to check for bottom
   * of image anyway (at row resolution), so no point in us doing it too.
   */

  /* Feed the postprocessor */
  (*cinfo->post->post_process_data) (cinfo, main->buffer,
                                     &main->rowgroup_ctr, rowgroups_avail,
                                     output_buf, out_row_ctr, out_rows_avail);

  /* Has postprocessor consumed all the data yet? If so, mark buffer empty */
  if (main->rowgroup_ctr >= rowgroups_avail) {
    main->buffer_full = FALSE;
    main->rowgroup_ctr = 0;
  }
}

/*
 * Process some data.
 * This handles the case where context rows must be provided.
 */
// 0x801BDBF4
METHODDEF(void)
process_data_context_main(j_decompress_ptr cinfo,
                          JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
                          JDIMENSION out_rows_avail)
{
  my_main_ptr main = (my_main_ptr) cinfo->main;

  /* Read input data if we haven't filled the main buffer yet */
  if (! main->buffer_full) {
    if (! (*cinfo->coef->decompress_data) (cinfo,
                                           main->xbuffer[main->whichptr]))
      return;                   /* suspension forced, can do nothing more */
    main->buffer_full = TRUE;   /* OK, we have an iMCU row to work with */
    main->iMCU_row_ctr++;       /* count rows received */
  }

  /* Postprocessor typically will not swallow all the input data it is handed
   * in one call (due to filling the output buffer first).  Must be prepared
   * to exit and restart.  This switch lets us keep track of how far we got.
   * Note that each case falls through to the next on successful completion.
   */
  switch (main->context_state) {
  case CTX_POSTPONED_ROW:
    /* Call postprocessor using previously set pointers for postponed row */
    (*cinfo->post->post_process_data) (cinfo, main->xbuffer[main->whichptr],
                        &main->rowgroup_ctr, main->rowgroups_avail,
                        output_buf, out_row_ctr, out_rows_avail);
    if (main->rowgroup_ctr < main->rowgroups_avail)
      return;                   /* Need to suspend */
    main->context_state = CTX_PREPARE_FOR_IMCU;
    if (*out_row_ctr >= out_rows_avail)
      return;                   /* Postprocessor exactly filled output buf */
    /*FALLTHROUGH*/
  case CTX_PREPARE_FOR_IMCU:
    /* Prepare to process first M-1 row groups of this iMCU row */
    main->rowgroup_ctr = 0;
    main->rowgroups_avail = (JDIMENSION) (cinfo->min_DCT_scaled_size - 1);
    /* Check for bottom of image: if so, tweak pointers to "duplicate"
     * the last sample row, and adjust rowgroups_avail to ignore padding rows.
     */
    if (main->iMCU_row_ctr == cinfo->total_iMCU_rows)
      set_bottom_pointers(cinfo);
    main->context_state = CTX_PROCESS_IMCU;
    /*FALLTHROUGH*/
  case CTX_PROCESS_IMCU:
    /* Call postprocessor using previously set pointers */
    (*cinfo->post->post_process_data) (cinfo, main->xbuffer[main->whichptr],
                        &main->rowgroup_ctr, main->rowgroups_avail,
                        output_buf, out_row_ctr, out_rows_avail);
    if (main->rowgroup_ctr < main->rowgroups_avail)
      return;                   /* Need to suspend */
    /* After the first iMCU, change wraparound pointers to normal state */
    if (main->iMCU_row_ctr == 1)
      set_wraparound_pointers(cinfo);
    /* Prepare to load new iMCU row using other xbuffer list */
    main->whichptr ^= 1;        /* 0=>1 or 1=>0 */
    main->buffer_full = FALSE;
    /* Still need to process last row group of this iMCU row, */
    /* which is saved at index M+1 of the other xbuffer */
    main->rowgroup_ctr = (JDIMENSION) (cinfo->min_DCT_scaled_size + 1);
    main->rowgroups_avail = (JDIMENSION) (cinfo->min_DCT_scaled_size + 2);
    main->context_state = CTX_POSTPONED_ROW;
  }
}

/*
 * Process some data.
 * Final pass of two-pass quantization: just call the postprocessor.
 * Source data will be the postprocessor controller's internal buffer.
 */
#ifdef QUANT_2PASS_SUPPORTED

// 0x801BDDB4
METHODDEF(void)
process_data_crank_post(j_decompress_ptr cinfo,
                        JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
                        JDIMENSION out_rows_avail)
{
  (*cinfo->post->post_process_data) (cinfo, (JSAMPIMAGE) NULL,
                                     (JDIMENSION *) NULL, (JDIMENSION) 0,
                                     output_buf, out_row_ctr, out_rows_avail);
}

#endif /* QUANT_2PASS_SUPPORTED */

/*
 * Initialize main buffer controller.
 */
// 0x801BDDDC
GLOBAL(void)
jinit_d_main_controller(j_decompress_ptr cinfo, boolean need_full_buffer)
{
  my_main_ptr main;
  int ci, rgroup, ngroups;
  jpeg_component_info *compptr;

  main = (my_main_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                SIZEOF(my_main_controller));
  cinfo->main = (struct jpeg_d_main_controller *) main;
  main->pub.start_pass = start_pass_main;

  if (need_full_buffer)         /* shouldn't happen */
    ERREXIT(cinfo, JERR_BAD_BUFFER_MODE);

  /* Allocate the workspace.
   * ngroups is the number of row groups we need.
   */
  if (cinfo->upsample->need_context_rows) {
    if (cinfo->min_DCT_scaled_size < 2) /* unsupported, see comments above */
      ERREXIT(cinfo, JERR_NOTIMPL);
    alloc_funny_pointers(cinfo); /* Alloc space for xbuffer[] lists */
    ngroups = cinfo->min_DCT_scaled_size + 2;
  } else {
    ngroups = cinfo->min_DCT_scaled_size;
  }

  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    rgroup = (compptr->v_samp_factor * compptr->DCT_scaled_size) /
      cinfo->min_DCT_scaled_size; /* height of a row group of component */
    main->buffer[ci] = (*cinfo->mem->alloc_sarray)
                        ((j_common_ptr) cinfo, JPOOL_IMAGE,
                         compptr->width_in_blocks * compptr->DCT_scaled_size,
                         (JDIMENSION) (rgroup * ngroups));
  }
}
