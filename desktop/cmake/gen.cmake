# Runs one of the project's generators and writes what it prints to OUT, as the client Makefile's
# "python3 tool.py args > file" rules do (custom commands can't redirect output portably).
#   cmake -DPYTHON=python3 -DOUT=file -DARGS="tool.py;arg;..." -P gen.cmake
get_filename_component(dir ${OUT} DIRECTORY)
file(MAKE_DIRECTORY ${dir})  # the build folder's gen/, on a first build (Ninja makes it, Make doesn't)
execute_process(COMMAND ${PYTHON} ${ARGS} OUTPUT_FILE ${OUT}.tmp RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  file(REMOVE ${OUT}.tmp)
  message(FATAL_ERROR "${ARGS}: failed (${rc})")
endif()
file(COPY_FILE ${OUT}.tmp ${OUT} ONLY_IF_DIFFERENT)
file(REMOVE ${OUT}.tmp)
