add_executable(bananagrams_web bindings.cpp)
target_link_libraries(bananagrams_web PRIVATE bananagrams::engine bananagrams_options)
# Catch C++ input/initialization errors in the worker without aborting the module.
target_compile_options(bananagrams_engine PRIVATE -fexceptions)
target_compile_options(bananagrams_web PRIVATE -fexceptions)
target_link_options(bananagrams_web PRIVATE
    --bind -fexceptions -sMODULARIZE=1 -sEXPORT_ES6=1
    -sENVIRONMENT=worker -sALLOW_MEMORY_GROWTH=1 -sFILESYSTEM=0
    -sEXPORT_EXCEPTION_HANDLING_HELPERS=1 -sEXPORTED_RUNTIME_METHODS=stackSave,stackRestore)
set_target_properties(bananagrams_web PROPERTIES
    OUTPUT_NAME bananagrams SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web/public/engine")
