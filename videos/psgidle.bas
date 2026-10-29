' psgidle.bas -- an idle cartridge for psg.py: the program does nothing, and jzIntv's debugger
' writes the PSG's registers once a frame, stopped at the breakpoint on tick (label_TICK)
    ON FRAME GOSUB tick
idle:
    WAIT
    GOTO idle

tick: PROCEDURE
    END
