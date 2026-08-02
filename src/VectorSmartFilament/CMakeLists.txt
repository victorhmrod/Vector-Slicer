find_package(SQLite3 REQUIRED)

set(vector_smart_filament_sources
    Application/FilamentService.cpp
    Application/FilamentProfileService.cpp
    Application/FilamentSyncService.cpp
    Application/GCodeConsumption.cpp
    Cache/LocalFilamentCache.cpp
    Core/FilamentSpool.cpp
    Core/FilamentTag.cpp
    Network/VectorErpFilamentClient.cpp
    NFC/FilamentTagProvider.cpp
    Persistence/SQLiteFilamentRepository.cpp
)

add_library(VectorSmartFilament STATIC ${vector_smart_filament_sources})
add_library(Vector::SmartFilament ALIAS VectorSmartFilament)

target_include_directories(VectorSmartFilament PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(VectorSmartFilament PUBLIC nlohmann_json PRIVATE SQLite::SQLite3 OpenSSL::Crypto libcurl)
target_compile_features(VectorSmartFilament PUBLIC cxx_std_17)
