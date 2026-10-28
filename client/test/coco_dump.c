/* autoplay's dump hook in the CoCo's test build (make coco-auto): it does
 * nothing. A debugger stops on it and saves the screen (tools/xroar_dump.py,
 * through xroar's GDB port), as atari800's monitor does on the Atari's BRK. */
void pc_dump(void)
{
}
