cmake_minimum_required( VERSION 3.10.0 )
#Set automoc and autouic policy
cmake_policy(PUSH)
if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
    if(NOT POLICY_SET) #less messages
        message(STATUS "CMP0071 policy set to NEW")
    endif()
endif()
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
    if(NOT POLICY_SET) #less messages
        message(STATUS "CMP0074 policy set to NEW")
    endif()
endif()
if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
    if(NOT POLICY_SET) #less messages
        message(STATUS "CMP0072 policy set to NEW")
    endif()
endif()
cmake_policy(POP)
set(POLICY_SET ON)
