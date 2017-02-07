server="salat2"
sys="mysql"
relFname=""

place_instrumented_file() {
    #scp ${server}:~ $1
    # Strip rel path from path
    fname=${relFname##*/}
    ssh ${server} /home/jiamin/zsh/bin/zsh <<EOF
    server_fname=$(find . -name $fname) 
EOF
    scp ${relFname} ${server}:${serverFname}
}

rebuild_mysql() {
    ssh ${server} /home/jiamin/zsh/bin/zsh <<EOF
    export LD_LIBRARY_PATH=/home/jiamin/gcc/lib64:$LD_LIBRARY_PATH
    /home/jiamin/mysql/support-files/mysql stop
    cd ${sys}-5.6.23/build
    make -j 64 install
EOF
}

place_instrumented_file
rebuild_mysql
