server="salat2"
sys="mysql"
relFname="sql_parse.cc"

place_instrumented_file() {
    #scp ${server}:~ $1
    # Strip rel path from path
    fname=${relFname##*/}
    scp ${relFname} ${server}:${sys}-5.6.23/sql/${relFname}
}

rebuild_mysql() {
    ssh ${server} /home/jiamin/zsh/bin/zsh <<EOF
    export LD_LIBRARY_PATH=/home/jiamin/gcc/lib64:$LD_LIBRARY_PATH
    /home/jiamin/mysql/support-files/mysql stop
    make -j 64 install -C /home/jiamin/${sys}-5.6.23/build/5.6-release/
EOF
}

place_instrumented_file
rebuild_mysql
