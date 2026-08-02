#ifndef _common_func_hppp_
#define _common_func_hppp_
#include <iostream>


#define SLIC3R_APP_NAME "OrcaSlicer-FullSpectrum"
// SLIC3R_APP_KEY defines the per-user configuration directory (wxWidgets
// SetAppName). It must stay distinct from "Snapmaker_Orca" and "OrcaSlicer" so
// this fork never shares/clobbers the configuration of those installs.
#define SLIC3R_APP_KEY "OrcaSlicer_FullSpectrum"
#define SLIC3R_VERSION "01.10.01.70"
#define Snapmaker_VERSION "2.3.1"
#define FULLSPECTRUM_VERSION "0.1.0-mvp"
#define MIN_FIRM_VER "1.0.0"
#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "0000000" // 0000000 means uninitialized
#endif
#define SLIC3R_BUILD_ID "2.3.1"
// #define SLIC3R_RC_VERSION "01.10.01.50"
#define BBL_RELEASE_TO_PUBLIC 1
#define BBL_INTERNAL_TESTING 0
#define ORCA_CHECK_GCODE_PLACEHOLDERS 0

namespace common 
{
	std::string get_pc_name();

	std::string get_flutter_version();

	std::string get_profile_version();

	std::string getMachineId();

	std::string getLocalArea();

	std::string getLanguage();

    } // namespace common

#endif
