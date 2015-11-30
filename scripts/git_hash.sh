#!/bin/bash

if [ -z ${1} ]; then
	echo "should be called with a path"
	exit
fi
ROOTDIR=${1}

CUSTOM_STR=${CUSTOM_STR:-https://git.linaro.org/lng/odp.git}
if [ -d ${ROOTDIR}/.git ]; then
	hash=$(git --git-dir=${ROOTDIR}/.git describe | tr -d "\n")
	if git --git-dir=${ROOTDIR}/.git diff-index --name-only HEAD &>/dev/null ; then
		dirty=-dirty
	fi

	SCMVERSION="'${CUSTOM_STR}' (${hash}${dirty})"
	echo -n $SCMVERSION >${ROOTDIR}/.scmversion
	echo -n $SCMVERSION
elif [ -e ${ROOTDIR}/.scmversion ]; then
	cat ${ROOTDIR}/.scmversion
else
	echo "should be called with a path"
	exit
fi

