cmake_minimum_required( VERSION 3.1.0 )

find_program(ASTYLE_BIN astyle DOC "Path to astyle binary")
if(ASTYLE_BIN)
    add_custom_target(fix-codestyle
        COMMAND ${ASTYLE_BIN}
        --options=${PROJECT_SOURCE_DIR}/psi.astylerc
        --exclude=${PROJECT_SOURCE_DIR}/3rdparty
        -R -Q
        *.cpp *.h *.c
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Run astyle application to fix project codestyle"
        VERBATIM
    )
endif()
