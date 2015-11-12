#=============================================================================
# Copyright (C) 2012-2013 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Pier Luigi Fiorini nor the names of his
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)

# wayland_add_protocol_client(outfiles inputfile basename)
function(WAYLAND_ADD_PROTOCOL_CLIENT _sources _protocol _basename)
    if(NOT WAYLAND_SCANNER_EXECUTABLE)
        message(FATAL "The wayland-scanner executable has nto been found on your system. You must install it.")
    endif()

    get_filename_component(_infile ${_protocol} ABSOLUTE)
    set(_client_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-${_basename}-client-protocol.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-${_basename}-protocol.c")

    add_custom_command(OUTPUT "${_client_header}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header < ${_infile} > ${_client_header}
        DEPENDS ${_infile} VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} code < ${_infile} > ${_code}
        DEPENDS ${_infile} VERBATIM)

    list(APPEND ${_sources} "${_client_header}" "${_code}")
    set(${_sources} ${${_sources}} PARENT_SCOPE)
endfunction()

# wayland_add_protocol_server(outfiles inputfile basename)
function(WAYLAND_ADD_PROTOCOL_SERVER _sources _protocol _basename)
    if(NOT WAYLAND_SCANNER_EXECUTABLE)
        message(FATAL "The wayland-scanner executable has nto been found on your system. You must install it.")
    endif()

    get_filename_component(_infile ${_protocol} ABSOLUTE)
    set(_server_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-${_basename}-server-protocol.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-${_basename}-protocol.c")

    add_custom_command(OUTPUT "${_server_header}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} server-header < ${_infile} > ${_server_header}
        DEPENDS ${_infile} VERBATIM)

    add_custom_command(OUTPUT "${_code}"
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} code < ${_infile} > ${_code}
        DEPENDS ${_infile} VERBATIM)

    list(APPEND ${_sources} "${_server_header}" "${_code}")
    set(${_sources} ${${_sources}} PARENT_SCOPE)
endfunction()
