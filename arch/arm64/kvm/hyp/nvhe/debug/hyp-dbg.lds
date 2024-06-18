SECTIONS {
  . = 0;
 .text : { _dbg_text_start = .; __hyp_section_.hyp.text = .; *(.text .text.*) }
 .hyp.data..ro_after_init : { __hyp_section_.hyp.data..ro_after_init = .; *(.data..ro_after_init .data..ro_after_init.*) }
 .hyp.rodata : { __hyp_section_.hyp.rodata = .; *(.rodata .rodata.*) }
 . = ALIGN((1 << 12));
 .hyp.data..percpu : { __hyp_section_.hyp.data..percpu = .;
  __per_cpu_start = .; *(.data..percpu..first) . = ALIGN((1 << 12)); *(.data..percpu..page_aligned) . = ALIGN((1 << (6))); *(.data..percpu..read_mostly) . = ALIGN((1 << (6))); *(.data..percpu) *(.data..percpu..shared_aligned) __per_cpu_end = .;
 }

 .bss : { _dbg_bss_start = .; __hyp_section_.hyp.bss = .; *(.bss .bss.*) }
 .data : { __hyp_section_.hyp.data = .; *(.data .data.*) }
}