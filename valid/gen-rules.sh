#!/bin/bash
function _make_target_extract_script()
{
    local mode="$1"
    shift

    local prefix="$1"
    local prefix_pat=$( printf "%s\n" "$prefix" | \
                        sed 's/[][\,.*^$(){}?+|/]/\\&/g' )
    local basename=${prefix##*/}
    local dirname_len=$(( ${#prefix} - ${#basename} ))

    if [[ $mode == -d ]]; then
        # display mode, only output current path component to the next slash
        local output="\2"
    else
        # completion mode, output full path to the next slash
        local output="\1\2"
    fi

    cat <<EOF
    1,/^# Files/                  d;            # skip until files section
    /^# Not a target/,/^$/        d;            # skip not target blocks
    /^${prefix_pat}/,/^$/!        d;            # skip anything user dont want

    # The stuff above here describes lines that are not
    #  explicit targets or not targets other than special ones
    # The stuff below here decides whether an explicit target
    #  should be output.

    /^# File is an intermediate prerequisite/ {
      s/^.*$//;x;                               # unhold target
      d;                                        # delete line
    }

    /^$/ {                                      # end of target block
      x;                                        # unhold target
      /^$/d;                                    # dont print blanks
      s|^\(.\{${dirname_len}\}\)\(.\{${#basename}\}[^:/]*/\{0,1\}\)[^:]*:.*$|${output}|p;
      d;                                        # hide any bugs
    }

    # This pattern includes a literal tab character as \t is not a portable
    # representation and fails with BSD sed
    /^[^#	:%]\{1,\}:/ {         # found target block
      /^\.PHONY:/                 d;            # special target
      /^\.SUFFIXES:/              d;            # special target
      /^\.DEFAULT:/               d;            # special target
      /^\.PRECIOUS:/              d;            # special target
      /^\.INTERMEDIATE:/          d;            # special target
      /^\.SECONDARY:/             d;            # special target
      /^\.SECONDEXPANSION:/       d;            # special target
      /^\.DELETE_ON_ERROR:/       d;            # special target
      /^\.IGNORE:/                d;            # special target
      /^\.LOW_RESOLUTION_TIME:/   d;            # special target
      /^\.SILENT:/                d;            # special target
      /^\.EXPORT_ALL_VARIABLES:/  d;            # special target
      /^\.NOTPARALLEL:/           d;            # special target
      /^\.ONESHELL:/              d;            # special target
      /^\.POSIX:/                 d;            # special target
      /^\.NOEXPORT:/              d;            # special target
      /^\.MAKE:/                  d;            # special target

      /^[^a-zA-Z0-9]/             d;            # convention for hidden tgt

      h;                                        # hold target
      d;                                        # delete line
    }

EOF
}

make -npq __BASH_MAKE_COMPLETION=1   .DEFAULT 2> /dev/null | sed -nf <(_make_target_extract_script -d $1)
