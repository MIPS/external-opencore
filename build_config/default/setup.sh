function echo_stdout()
{
   echo -e "[setup.sh] $@"
}

function print_menu()
{
    echo_stdout
    echo_stdout "Build selection menu... choose from the following:"
    echo_stdout
    echo_stdout "1. Build for host platform"
    echo_stdout "2. Generate Android.mk files"
    echo_stdout "3. Build using default linux-arm cross-compiler"
    echo_stdout

}

function clean_env()
{
  echo_stdout "=================================="
  echo_stdout "Cleaning ARCHITECTURE"
  unset ARCHITECTURE
  echo_stdout "Setting PATH back to the original"
  export PATH=$BASE_PATH
  echo_stdout "Cleaning PROCESSOR"
  unset PROCESSOR
  echo_stdout "=================================="
}

function menu()
{
    if [ "$1" ] ; then
        CHOICE=$1
    else
        print_menu
        read -p "[setup.sh] Which selection would you like? " CHOICE
    fi

    case $CHOICE in
    1)
        echo_stdout "Choice is to build for the host platform."
        clean_env
        ;;
    2)
        echo_stdout "Choice is to do Android.mk generation."
        ## clean the environment
        clean_env
        export ARCHITECTURE=android
        export PROCESSOR=arm
        echo_stdout "ARCHITECTURE set to ==> $ARCHITECTURE"
        ;;
    3)
        echo_stdout "Choice is to build for target with the default linux-arm cross-compiler"
        # clean the environment
        clean_env
        # set path up for linux-arm compiler
        linux_arm_path=/opt/environments/linux_arm/data/omapts/linux/arm-tc/gcc-3.4.0-1/bin
        export ARCHITECTURE=linux_arm
        export PATH=$linux_arm_path:$BASE_PATH
        ;;
    *)
        echo_stdout "Invalid selection.  Please enter your selection again."
        print_menu
        return
        ;;
    esac
}

function mkcmdcmpl()
{
    echo_stdout "getting make cmdline completion values"
    export PV_MAKE_COMPLETION_TARGETS=`make -j completion_targets`
    echo_stdout "done getting make cmdline completion values."
}

function get_host_platform()
{
    local tmp_host
    tmp_host=`uname`
    case $tmp_host in
    Linux)
       tmp_host="linux"
       ;;
    Darwin)
       tmp_host="mac"
       ;;
    *)
       echo_stdout "*** WARNING - host platform $tmp_host unknown"
       tmp_host="unknown"
       ;;
    esac
    eval "$1=$tmp_host"
}

echo_stdout "started."
echo_stdout "setting up build environment with default configuration"

export PROJECT_DIR=$PWD

if [[ $# -ge 1 ]]; then
  export BASE_DIR=${1%/}
fi

if [[ -z $BASE_DIR ]]; then
  echo_stdout "ERROR: BASE_DIR is not set!"
fi
echo_stdout "BASE_DIR               ==> $BASE_DIR"

export SRC_ROOT=$BASE_DIR
echo_stdout "SRC_ROOT               ==> $SRC_ROOT"

export BUILD_ROOT=$PROJECT_DIR/build
echo_stdout "BUILD_ROOT             ==> $BUILD_ROOT"

export CFG_DIR=$PWD
echo_stdout "CFG_DIR                ==> $CFG_DIR"

export MK=$BASE_DIR/tools_v2/build/make
echo_stdout "MK                     ==> $MK"

export ARTISTIC_STYLE_OPTIONS=$BASE_DIR/tools_v2/editor_settings/astylerc
echo_stdout "ARTISTIC_STYLE_OPTIONS ==> $ARTISTIC_STYLE_OPTIONS"

get_host_platform host
extern_tools_path=$BASE_DIR/extern_tools_v2/bin/$host

export PATH=$extern_tools_path:$PATH
export BASE_PATH=$PATH

_pv_make_completion()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    opts="${PV_MAKE_COMPLETION_TARGETS}"

    case "${prev}" in 
      -f)
        COMPREPLY=( $(compgen -f ${cur}) )
        return 0
        ;;
    *)
        COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
        return 0
        ;;
    esac
}

complete -F _pv_make_completion make
###

echo_stdout
echo_stdout "environment is ready if no errors reported."
echo_stdout "complete."
