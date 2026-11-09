#ifndef RF_XBOX_IGR_H
#define RF_XBOX_IGR_H

/* In-game reset (IGR): back to the Xbox dashboard by a quick reboot, as
 * softmod IGR does. Used by the main menu's EXIT GAME and by holding
 * LT + RT + Back + Black on the controller in any game state. Never returns
 * (a full reboot is the fallback when the launch page cannot be allocated). */
void rf_xbox_igr_reset(void);

#endif
