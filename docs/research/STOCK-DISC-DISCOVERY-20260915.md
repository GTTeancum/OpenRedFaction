# Stock disc discovery and registry dependency (2026-09-15)

Read-only disassembly of original RF.exe, SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No game launch, mounted executable execution, registry edits, disc/access-check patches or media modifications.

## Exact required media marker

Startup4b31a0 passes string5a1858 **RF_2** to scanner4b2ed0. The scanner enumerates drivelettersA..Z, callingGetDriveTypeA and acceptingtype5(optical). It callsGetVolumeInformationA and requires successfulreturn. It then tests two files atdriveROOT:

- RedFaction.ico (literal5a17d8): disc1marker.
- music.vpp (literal5a17e8): disc2marker.

Both use CRTfindfirst575b75 and accept a handle otherthan-1, thenclose575d0a. At4b307b it compares requestedtag againstRF_1;4b3099 comparesRF_2. **RF_2 is a requested category token, not a proved volume-label requirement.** The scanner retrievesvolumeinformation but the inspected routine does not compare its labelbuffer toRF_2 oranotherlabel. Therefore renaming a volume is not a supported fix.

ForRF_2 the requiredmatchingmarker isrootmusic.vpp. Successfuldriveindex is converted4b3140 into"%c:\\" at7d95a0 andpassed5156c0. Startupthen4b3238..4b323f registers music.vpp through52c070. This is file discovery and subsequent assetregistration; amarker alone does not prove assetvalidity. Do not fabricate a marker to bypass missingmedia.

## Why the empty registry is not the established cause

The complete inspected4b2ed0 discoveryroutine contains no registrywrapper calls. Its drive/path selection is basedonopticaltype,volumeAPI success, androotmarker files.4b31a0 alsohas noinstallation/CDpath registrylookup. Accordingly no exact registrykey/value canbe supplied as a demonstrated remedy; inventing InstallPath/CDPath would not address this path.

The privatecopy alreadyhaslocalmusic.vpp, butthisstartupscanner explicitlyprobesopticaldrive roots. Ordinary file workingdirectory/privateinstallationsettings are separatefromtheobserveddisc2prompt. The scanner savescurrentdirectory thenrestoresit before returning.

## Observed-run status from primary

User screenshotshowed Insert Red Faction CD #2. MountedE root was reportedasAutoRun.exe,Autorun.ico,Autorun.inf,data3.cab,andCrackdirectory;volumeRFACTION2_2. That listing lacksrequiredrootmusic.vpp. This explains whythismountedroot cannot pass the inspectedstockRF_2 discovery, withoutproving which productthemedia contains. The labelalone doesnot prove Red Faction2 oranotherrelease. Nothingonthatmedia was executedbythisagent.

Useful next step: user-provided correctoriginalRedFaction(2001) disc2orverifiedcompatiblemedia withitsauthenticassets. Do notchangethecheck orcopy amarker simplytomake itpass. Originalassets/registryremainuntouched.
