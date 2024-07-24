#!/usr/local/bin/ksh93 -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)zfs_acl_chmod_compact_001_pos.ksh	1.5	09/01/13 SMI"
#

. $STF_SUITE/tests/acl/acl_common.kshlib
. $STF_SUITE/tests/acl/cifs/cifs.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: zfs_acl_chmod_compact_001_pos
#
# DESCRIPTION:
#	chmod A{+|-|=} could set compact ACL correctly.
#
# STRATEGY:
#	1. Loop root and non-root user.
#	2. Get the random compact ACL string.
#	4. Separately chmod +|-|=
#	5. Check compact ACL display as expected 
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-08-11)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "both"

test_requires ZFS_ACL

log_assert "chmod A{+|=} should set compact ACL correctly."
log_onexit cleanup

set -A a_flag owner group everyone
set -A a_access r w x p d D a A R W c C o s 
set -A a_inherit_object f d
set -A a_inherit_strategy i n
set -A a_type allow deny

typeset cifs=""

if cifs_supported ; then
	cifs="true"
fi

#
# Get a random item from an array.
#
# $1 the base set
#
function random_select #array_name
{
	typeset arr_name=$1
	typeset -i ind

	eval typeset -i cnt=\${#${arr_name}[@]}
	(( ind = $RANDOM % cnt ))

	eval print \${${arr_name}[$ind]}
}

#
# Create a random string according to array name, the item number and 
# separated tag.
#
# $1 array name where the function get the elements
# $2 the items number which you want to form the random string
# $3 the separated tag
#
function form_random_str #<array_name> <count> <sep>
{
	typeset arr_name=$1
	typeset -i count=${2:-1}
	typeset sep=${3:-""}

	typeset str=""
	while (( count > 0 )); do
		str="${str}$(random_select $arr_name)${sep}"

		(( count -= 1 ))
	done

	print $str
}

#
# According to the input ACE access,ACE type, and inherit flags, return the
# expect compact ACE that could be used by chmod A0{+|=}'.
#
# $1 ACE flag which is owner, group, or everyone
# $2 ACE access generated by the element of a_access
# $3 ACE inherit_object generated by the element of a_inherit_object
# $4 ACE inherit_strategy generated by the element of a_inherit_strategy
# $5 ACE type which is allow or deny
#
function cal_ace # acl_flag acl_access \
		 # acl_inherit_object acl_inherit_strategy acl_type
{
	typeset acl_flag=$1
	typeset acl_access=$2
	typeset acl_inherit_object=$3
	typeset acl_inherit_strategy=$4
	typeset acl_type=$5

	tmp_ace=${acl_flag}@:

	for element in ${a_access[@]} ; do
		if [[ $acl_access == *"$element"* ]]; then
			tmp_ace="${tmp_ace}${element}"
		else
			tmp_ace="${tmp_ace}-"
		fi
	done
	tmp_ace=${tmp_ace}:

	for element in ${a_inherit_object[@]} ; do
		if [[ $acl_inherit_object == *"$element"* ]]; then
			tmp_ace="${tmp_ace}${element}"
		else
			tmp_ace="${tmp_ace}-"
		fi
	done
	for element in ${a_inherit_strategy[@]} ; do
		if [[ $acl_inherit_strategy == *"$element"* ]]; then
			tmp_ace="${tmp_ace}${element}"
		else
			tmp_ace="${tmp_ace}-"
		fi
	done

	if [[ -n $cifs ]]; then
		tmp_ace=${tmp_ace}---:${acl_type}
	else
		tmp_ace=${tmp_ace}--:${acl_type}
	fi
	
	print "${tmp_ace}"
}

#
# Check if chmod set the compact ACE correctly.
#
function check_test_result # node acl_flag acl_access \
			   # acl_inherit_object acl_inherit_strategy acl_type
{
	typeset node=$1
	typeset acl_flag=$2
	typeset acl_access=$3
	typeset acl_inherit_object=$4
	typeset acl_inherit_strategy=$5
	typeset acl_type=$6

	typeset expect_ace=$(cal_ace "$acl_flag" "$acl_access" \
		"$acl_inherit_object" "$acl_inherit_strategy" "$acl_type")	

	typeset cur_ace=$(get_ACE $node 0 "compact")

	if [[ $cur_ace != $expect_ace ]]; then
		log_fail "FAIL: Current map($cur_ace) !=  \
			expected ace($expect_ace)"
	fi
}

function test_chmod_map #<node>
{
	typeset node=$1	
	typeset acl_flag acl_access acl_inherit_object acl_inherit_strategy acl_type
	typeset -i cnt

	if (( ${#node} == 0 )); then
		log_fail "FAIL: file name or directroy name is not defined."
	fi

        # Get ACL flag, access & type
	acl_flag=$(form_random_str a_flag)
	(( cnt = ($RANDOM % ${#a_access[@]}) + 1 ))
	acl_access=$(form_random_str a_access $cnt)
	acl_access=${acl_access%/}
	acl_type=$(form_random_str a_type 1)

	acl_spec=${acl_flag}@:${acl_access}
	if [[ -d $node ]]; then
		# Get ACL inherit_object & inherit_strategy
		(( cnt = ($RANDOM % ${#a_inherit_object[@]}) + 1 ))
		acl_inherit_object=$(form_random_str a_inherit_object $cnt)
		(( cnt = ($RANDOM % ${#a_inherit_strategy[@]}) + 1 ))
		acl_inherit_strategy=$(form_random_str a_inherit_strategy $cnt)
		acl_spec=${acl_spec}:${acl_inherit_object}${acl_inherit_strategy}
	fi
	acl_spec=${acl_spec}:${acl_type}

	# Set the initial map and back the initial ACEs
	typeset orig_ace=$TMPDIR/orig_ace.${TESTCASE_ID}
	typeset cur_ace=$TMPDIR/cur_ace.${TESTCASE_ID}

	for operator in "A0+" "A0="; do
		log_must usr_exec eval "$LS -Vd $node > $orig_ace"

		# To "A=", firstly add one ACE which can't modify map
		if [[ $operator == "A0=" ]]; then
			log_must $CHMOD A0+user:$ZFS_ACL_OTHER1:execute:deny \
				$node
		fi
		log_must usr_exec $CHMOD ${operator}${acl_spec} $node
			
		check_test_result \
			"$node" "$acl_flag" "$acl_access" \
			"$acl_inherit_object" "$acl_inherit_strategy" \
			"$acl_type"

		# Check "chmod A-"
		log_must usr_exec $CHMOD A0- $node
		log_must usr_exec eval "$LS -Vd $node > $cur_ace"
	
		$DIFF $orig_ace $cur_ace
		[[ $? -ne 0 ]] && \
			log_fail "FAIL: 'chmod A-' failed."
	done

	[[ -f $orig_ace ]] && log_must usr_exec $RM -f $orig_ace
	[[ -f $cur_ace ]] && log_must usr_exec $RM -f $cur_ace
}

for user in root $ZFS_ACL_STAFF1; do
	set_cur_usr $user
	
	typeset -i loop_cnt=2
	while (( loop_cnt > 0 )); do
		log_must usr_exec $TOUCH $testfile
		test_chmod_map $testfile
		log_must $RM -f $testfile

		log_must usr_exec $MKDIR $testdir
		test_chmod_map $testdir
		log_must $RM -rf $testdir

		(( loop_cnt -= 1 ))
	done
done

log_pass "chmod A{+|=} set compact ACL correctly."
