SECTIONS
{
  .rtc.noload (NOLOAD) :
  {
    . = ALIGN(4);
    *(.rtc.noload) /* .rtc.noload section */
    . = ALIGN(4);
  } > rtc_slow_seg
}
