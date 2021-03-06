# tool chain for blueprint
SET(CMAKE_SYSTEM_PROCESSOR "P5p")
SET(QMC_ENV "IBMP5p" CACHE STRING "Setting envirnoments for IBM P5+")

SET(HAVE_MPI 0)
SET(ENABLE_OPENMP 1)
SET(HAVE_LIBESSL 1)
SET(IBM_COMPILER 1)

set(CMAKE_FIND_ROOT_PATH
    /home/jnkim/share/boost
    /home/jnkim/share/hdf5
    /home/jnkim/share/einspline
    /opt/fftw-3.2.2
)


#SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

add_definitions(-DINLINEALL= -DINLINE0= -DINLINE1= )
set(CMAKE_C_COMPILER /opt/ibmcmp/vacpp/11.1/bin/xlc_r)
set(CMAKE_CXX_COMPILER /opt/ibmcmp/vacpp/11.1/bin/xlC_r)
set(CMAKE_Fortran_COMPILER /opt/ibmcmp/xlf/13.1/bin/xlf_r)

#SET(AIX_ARCH_FLAGS "-q64 -qarch=pwr7 -qtune=pwr7 -qcache=auto -qsimd=auto -qaltivec -qsmp=omp -qthreaded")
SET(AIX_ARCH_FLAGS "-q64 -qarch=pwr7 -qtune=pwr7 -qcache=auto -qsimd -qinline -qsmp=omp -qthreaded")
SET(AIX_CXX_COMMON_FLAGS "-qnoeh -qsuppress=1540-1090:1540-1088 ")
#SET(AIX_OPT_FLAGS "-O3 -Q -qmaxmem=-1 -qipa=inline -qinline -qlargepage -qprefetch -qstrict -qhot -qkeyword=restrict")
SET(AIX_OPT_FLAGS "-O3 -Q -qmaxmem=-1 -qlargepage -qprefetch -qkeyword=restrict")
SET(F_DEFINES "-WF,-P,-DESSL,-DDOUBLE_PREC,-DSTRIDE1 -qfixed=132")
#SET(AIX_F_FLAGS "-O3 -Q -qmaxmem=-1 -qlargepage -qprefetch -qstrict -qhot")
SET(AIX_F_FLAGS "-O3 -Q -qmaxmem=-1 -qlargepage -qprefetch")
#SET(AIX_OPT_FLAGS "-g -O3 -qlargepage -qprefetch -qstrict -qhot -qkeyword=restrict")
#SET(AIX_OPT_FLAGS "-g -Q -qmaxmem=-1 -qlargepage -qprefetch -qkeyword=restrict")
#SET(AIX_C_FLAGS "-qlanglvl=stdc99")

######################################################################
#set the CXX flags: arch+common + opt 
######################################################################
SET(CMAKE_CXX_FLAGS "${AIX_ARCH_FLAGS} ${AIX_CXX_COMMON_FLAGS} ${AIX_OPT_FLAGS}")
SET(CMAKE_C_FLAGS "${AIX_ARCH_FLAGS} ${AIX_OPT_FLAGS} ${AIX_C_FLAGS}")
SET(CMAKE_Fortran_FLAGS "${F_DEFINES} ${AIX_ARCH_FLAGS} ${AIX_F_FLAGS}")
SET(CMAKE_Fortran_FLAGS_RELEASE ${CMAKE_Fortran_FLAGS})

#link_libraries(-L/users/jk22/share/lib64 -llapack -L/opt/apps/ibm/essl/5.1/lib64 -lessl -L/opt/apps/ibm/xlf/13.1/lib64 -lxlf90_r)
link_libraries(/home/gbauer/lib64/lapack_64.a  -lessl -L/opt/ibmcmp/xlmass/6.1/lib64 -lmass_simdp7_64 -L/opt/ibmcmp/xlf/13.1/lib64 -lxlf90_r -lxlsmp)
