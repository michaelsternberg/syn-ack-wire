' ===========================================================================
' isr.bas -- the frame handler (ON FRAME GOSUB frame_isr): IntyBASIC allows
' one, so the controller, the ECS keyboard, the chords and the title's
' static all hang off it. It runs before the program's own start-up (every
' variable is 0 then), and must stay well inside a frame and shallow: the
' stack is 24 words.
' ===========================================================================

frame_isr: PROCEDURE
    GOSUB read_input
    GOSUB ecs_tick
    GOSUB snd_isr
    IF snow_on THEN
        #snow = USR SNOW(#snow, title_snow_at(snow_part))
        snow_part = snow_part + 1
        IF snow_part = 3 THEN snow_part = 0
    END IF
END
