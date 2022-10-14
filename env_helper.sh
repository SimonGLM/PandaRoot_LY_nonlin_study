#either source this or paste this directly in .bashrc

############################
# Jesus, this is cursed...
# Maybe let's reason first why and what we want do do:
#
# In order to keep environments clean, we want to keep them in a new shell at level 2.
# We can load/enter a clean environment by calling e.g `SourcePandaRoot_v13' and close it by Crtl+D.
# We automatically set some evn vars and cd into the project dir.
# A new prompt (PS1) is also set, to show where we are
#
# The following function does nothing without arguments.
# Helper functions below export variable "version"
# Finally at the end, we call this function with "version" as argument
# On first sourcing of .bashrc nothing happens.
# We can then execute e.g. SourcePandaRoot_v13 to set "version" to apr22
# and start bash, which will then load this file again and finally call SourcePandaRoot_.
# Now that "version" has been set in SHLVL 1, we source the PandaRoot environment and output some stuff...
# SHLVL 2 is now ready for PandaRoot, and will not contaminate the environment in SHLVL 1.
# Anyone got a better idea to keep things tidy? -> housekeeping@siglm.de
function SourcePandaRoot_() {
        if [ -z $1 ];then
                # echo "Do nothin"
                return 0;
        fi
        PR_ROOT_DIR=$1
        source /home/simon/$PR_ROOT_DIR/PandaRoot/build/config.sh
        if [ $? -ne 0 ];then
                echo -e "$msg"
                echo -e "\e[01;31mFailed to source root config\e[0m"
                exit
        fi
        export FAIRSOFTVERSION=$(git -C /home/simon/$PR_ROOT_DIR/FairSoft/source branch | grep '*' | sed -n 's/\(\*\s\)\(.*\)/\2/p')
        export FAIRROOTVERSION=$(git -C /home/simon/$PR_ROOT_DIR/FairRoot/source branch  | grep '*' | sed -n 's/\(\*\s\)\(.*\)/\2/p')
        export PANDAROOTVERSION=$(git -C /home/simon/$PR_ROOT_DIR/PandaRoot/source branch  | grep '*' | sed -n 's/\(\*\s\)\(.*\)/\2/p')
        echo -e 'FairSoft version  \e[2m($FAIRSOFTVERSION) \e[0m |' $FAIRSOFTVERSION
        echo -e 'FairRoot version  \e[2m(FAIRROOTVERSION)  \e[0m |' $FAIRROOTVERSION
        echo -e 'PandaRoot version \e[2m($PANDAROOTVERSION)\e[0m |' $PANDAROOTVERSION
        echo -e 'Project directory \e[2m($PROJECTDIR)      \e[0m |' $PROJECTDIR
        alias root='root -l --web=off'
        cd $PROJECTDIR
        PS1="\[\e[01;32m\](L$SHLVL)\[\e[01;33m\]"$prompt"\[\e[0m\]:\[\e[01;34m\]\w\[\e[0m\]\$ "
        return 0
}
# In case you want to install more versions of PandaRoot, add them here:
function SourcePandaRoot_v13 () {
        export version="fairsoft_apr22"
        export prompt="[v13.0.0]"
        export PROJECTDIR=/home/simon/$version/project
        bash
}
function SourcePandaRoot_oct19 () {
        export version="fairsoft_jun19"
        export prompt="[oct19]"
        export PROJECTDIR=/home/simon/$version/PandaRoot/source/projects/LY_non-lin
        bash
}
SourcePandaRoot_ $version
