' ===========================================================================
' ecskbd.bas -- the ECS keyboard, when one is attached: a scan of its matrix
' every frame (ecskbd.asm, Joe Zbiciak's CC0 scanner), each key as it goes
' down into the key ring (input.bas), as the game's keys: letters upper
' case, RTN K_RETURN, the left arrow K_DELETE, ? K_HELP, ESC K_ESC, the
' other arrows the disc's. After netcat's (~/Workspace/netcat/intv/
' ecskbd.bas). Without an ECS, nothing here runs.
'
' tools/intvsim.py types on it headless: a key at ECS_HOOK stands in for
' the scanner's once (0 = none).
' ===========================================================================

    CONST ECS_NONE  = 255
    CONST ECS_ENTER = $0A
    CONST ECS_LEFT  = $1C
    CONST ECS_HOOK  = $8FF2

    DIM ecs_present             ' set at boot: 0 = no ECS
    DIM ecs_raw, ecs_plast      ' the frame handler's

' ECS.AVAILABLE is IntyBASIC's own probe at boot. Reading it doesn't mark the
' ROM as needing an ECS (CONT3, CONT4 or SOUND 5-9 would).
ecs_init: PROCEDURE
    POKE ECS_HOOK, 0
    ecs_plast = ECS_NONE
    ecs_present = ECS.AVAILABLE
END

' In the frame handler.
ecs_tick: PROCEDURE
    IF ecs_present = 0 THEN RETURN
    ecs_raw = rb(ECS_HOOK)
    IF ecs_raw THEN
        POKE ECS_HOOK, 0
        ecs_plast = ECS_NONE
    ELSE
        ecs_raw = USR ECSKEY
        IF ecs_raw = ecs_plast THEN RETURN
        ecs_plast = ecs_raw
        IF ecs_raw = ECS_NONE THEN RETURN
    END IF
    kq_k = ecs_raw
    IF kq_k >= 97 AND kq_k <= 122 THEN kq_k = kq_k - 32
    IF kq_k = ECS_ENTER THEN kq_k = K_RETURN
    IF kq_k = ECS_LEFT THEN kq_k = K_DELETE
    IF kq_k = 63 THEN kq_k = K_HELP
    GOSUB kq_push
END

    ' Use the ECS when it is there, and run without it (ECS-enhanced, not
    ' ECS-required): jzIntv then loads the ECS with this ROM.
    ASM CFGVAR "ecs_compat" = 2

    ASM INCLUDE "ecskbd.asm"
