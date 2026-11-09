/* In-game reset (see igr.h). Adapted from the author's Perfect Dark X port,
 * GTTeancum/Perfect-Dark-X port/src/input.c inputXboxReturnToDashboard
 * (commit 86566c0, MIT licence, copyright 2022 Ryan Dwyer and contributors):
 * a dashboard launch page, committed to RAM before the quick reboot. */
#include "igr.h"
#include "audio.h"
#include <hal/xbox.h>
#include <string.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

void rf_xbox_igr_reset(void)
{
    const ULONG page_size=0x1000;
    /* Silence the APU first so the reboot does not replay stale sound. */
    if(rf_xbox_audio_ready())rf_xbox_audio_close();
    if(!LaunchDataPage)LaunchDataPage=MmAllocateContiguousMemory(page_size);
    if(!LaunchDataPage)HalReturnToFirmware(HalRebootRoutine);
    MmPersistContiguousMemory(LaunchDataPage,page_size,TRUE);
    memset((void *)LaunchDataPage,0,page_size);
    LaunchDataPage->Header.dwLaunchDataType=LDT_LAUNCH_DASHBOARD;
    LaunchDataPage->Header.dwTitleId=CURRENT_XBE_HEADER->CertificateHeader->TitleID;
    LaunchDataPage->Header.dwFlags=0;
    /* Commit the page to RAM: XEMU's coherent memory hides stale cache lines
     * that real hardware can enter the firmware with (Perfect Dark X found). */
    __asm__ __volatile__("sfence" ::: "memory");
    __asm__ __volatile__("wbinvd" ::: "memory");
    HalReturnToFirmware(HalQuickRebootRoutine);
    for(;;)Sleep(1000);
}
