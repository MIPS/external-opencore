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
    echo_stdout "4. Build using Android arm-eabi 4.2.1"
    echo_stdout "5. Build using Android arm-eabi 4.3.1"
    echo_stdout "6. Build using Android arm-eabi 4.4.0"
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
  unset ANDROID_BASE_TYPE
  echo_stdout "=================================="
}

function get_android_prebuilt_path()
{
    local tmp_host
    tmp_host=`uname`
    case $tmp_host in
    Linux)
       tmp_host="linux-x86"
       ;;
    Darwin)
       tmp_host="darwin-x86"
       ;;
    *)
       echo_stdout "*** WARNING - host platform $tmp_host unknown - can't set android path correctly."
       tmp_host="unknown"
       ;;
    esac
    eval "$1=$tmp_host"
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
    4)
        echo_stdout "Choice is to build for target with android arm eabi 4.2.1 compiler"
        # clean the environment
        clean_env
	    # get the prebuilt path based on the host platform
	    local prebuilt_path
	    get_android_prebuilt_path prebuilt_path
        # set path up for the arm cross-compiler
        if [ -d $ANDROID_BASE/prebuilt/$prebuilt_path/toolchain ]; then
            # path in the android SDK
            echo_stdout "setting path based on Android SDK"
            export ANDROID_TOOLCHAIN_PATH=$ANDROID_BASE/prebuilt/$prebuilt_path/toolchain/arm-eabi-4.2.1
            export ANDROID_BASE_TYPE=SDK
        elif [ -d $ANDROID_BASE/build/prebuilt/$prebuilt_path/arm-eabi-4.2.1 ]; then
            # path in the android ndk
            echo_stdout "setting path based on Android NDK"
            export ANDROID_TOOLCHAIN_PATH=$ANDROID_BASE/build/prebuilt/$prebuilt_path/arm-eabi-4.2.1
            export ANDROID_BASE_TYPE=SDK
        else
            echo_stdout "Cross-compiler path not found!"
            echo_stdout "Please check if ANDROID_BASE is set correctly in your environment."
            return
        fi

        export ANDROID_TOOLCHAIN_PATH=$ANDROID_BASE/prebuilt/$prebuilt_path/toolchain/arm-eabi-4.2.1
        export ANDROID_GCC_VERSION=4.2.1
        android_arm_path=$ANDROID_TOOLCHAIN_PATH/bin
        export ARCHITECTURE=android_arm
        export PATH=$android_arm_path:$BASE_PATH
        ;;
    5)
        echo_stdout "Choice is to build for target with android arm eabi 4.3.1 compiler"
        # clean the environment
        clean_env
        # get the prebuilt path based on the host platform
        local prebuilt_path
        get_android_prebuilt_path prebuilt_path
        # set path up for the arm cross-compiler
        if [ -d $ANDROID_BASE/prebuilt/$prebuilt_path/toolchain ]; then
            # path in the android SDK
            echo_stdout "setting path based on Android SDK"
            export ANDROID_TOOLCHAIN_PATH=$ANDROID_BASE/prebuilt/$prebuilt_path/toolchain/arm-eabi-4.3.1
            export ANDROID_BASE_TYPE=SDK
        elif [ -d $ANDROID_BASE/build/prebuilt/$prebuilt_path/arm-eabi-4.3.1 ]; then
            # path in the android ndk
            echo_stdout "setting path based on Android NDK"
            export ANDROID_TOOLCHAIN_PATH=$ANDROID_BASE/build/prebuilt/$prebuilt_path/arm-eabi-4.3.1
            export ANDROID_BASE_TYPE=NDK
        else
            echo_stdout "Cross-compiler path not found!"
            echo_stdout "Please check if ANDROID_BASE is set correctly in your environment."
            return
        fi

	    export ANDROID_GCC_VERSION=4.3.1
        android_arm_path=$ANDROID_TOOLCHAIN_PATH/bin
        export ARCHITECTURE=android_arm
        export PATH=$android_arm_path:$BASE_PATH
        ;;
    6)
        echo_stdout "Choice is to build for target with android arm eabi 4.4.0 compiler"
        # clean the environment
        clean_env
        # get the prebuilt path based on the host platform
        local prebuilt_path
        get_android_prebuilt_path prebuilt_path
        if [ -d $ANDROID_BASE/prebuilt/$prebuilt_path/toolchain ]; then
            # path in the android SDK
            echo_stdout "setting path based on Android SDK"
            export ANDROID_TOOLCHAIN_PATH=$ANDROID_BASE/prebuilt/$prebuilt_path/toolchain/arm-eabi-4.4.0
            export ANDROID_BASE_TYPE=SDK
        elif [ -d $ANDROID_BASE/build/prebuilt/$prebuilt_path/arm-eabi-4.4.0 ]; then
            # path in the android ndk
            echo_stdout "setting path based on Android NDK"
            export ANDROID_TOOLCHAIN_PATH=$ANDROID_BASE/build/prebuilt/$prebuilt_path/arm-eabi-4.4.0
            export ANDROID_BASE_TYPE=NDK
        else
            echo_stdout "Cross-compiler path not found!"
            echo_stdout "Please check if ANDROID_BASE is set correctly in your environment."
            return
        fi

	    export ANDROID_GCC_VERSION=4.4.0
        android_arm_path=$ANDROID_TOOLCHAIN_PATH/bin
        export ARCHITECTURE=android_arm
        export PATH=$android_arm_path:$BASE_PATH
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

function get_base_dir
{
    local BASE_DIR_SETUP=build_config/default/setup.sh
    if [ -n "$BASE_DIR" -a -f "$BASE_DIR/$BASE_DIR_SETUP" ] ; then 
        echo $BASE_DIR
    else
        if [ -f $BASE_DIR_SETUP ] ; then
            PWD= /bin/pwd
        else
            local CUR_DIR=$PWD
            DIR=
            while [ \( ! \( -f $BASE_DIR_SETUP \) \) -a \( $PWD != "/" \) ]; do
                cd .. > /dev/null
                DIR=`PWD= /bin/pwd`
            done
            cd $CUR_DIR > /dev/null
            if [ -f "$DIR/$BASE_DIR_SETUP" ]; then
                echo $DIR
            fi
        fi
    fi
}


echo_stdout "started."
echo_stdout "setting up build environment with default configuration"

export PROJECT_DIR=$PWD

if [[ $# -ge 1 ]]; then
  export BASE_DIR=${1%/}
fi

if [[ -z $BASE_DIR ]]; then
  BASE_DIR=$(get_base_dir)
fi

if [[ -z $BASE_DIR ]]; then
  echo_stdout "ERROR: BASE_DIR is not set!"
  return
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

export MK_ENTRY_EXTRAS=$MK/gitignore.mk
echo_stdout "MK_ENTRY_EXTRAS        ==> $MK_ENTRY_EXTRAS"

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
