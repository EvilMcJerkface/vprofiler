#!/bin/bash

###########################################################
# Command line arguments                                  #
# $1 is the name of the server to connect to              #
# $2 is the suffix to append names on files to            #
# $3 is the name of the system to run with                #
# $4 is the relative path to the instrumented file
###########################################################

time=1m
config=original.xml
database=large_scale
experiment_name="ETF_sum_age"
do_rebuild=false
do_reset_db=true
buffer_size=30
flush=1
start=1
end=1
server=$1
suffix=$2
sys=$3
instrumented_file_path=$4
rate=100
warehouse=128

place_instrumented_file() {
    #scp ${server}:~ $1
    # Strip rel path from path
    fname=${1##*/}
    ssh ${server} /home/jiamin/zsh/bin/zsh <<EOF
    server_fname=$(find . -name $fname) 
    exit
    scp $1 ${server}:${serverFname}
EOF
}

rebuild_mysql() {
    ssh ${server} /home/jiamin/zsh/bin/zsh <<EOF
    cd ${sys}-5.6.23/build
    make -j 64 install
EOF
}

rm_old_results() {
    rm -rf ~/oltpbench/results/exp*
    # ssh salat2 "rm -rf oltpbench/results/$1*"
}

wait_for_process() {
    pid=`ps aux | grep $1 | grep -v grep | awk -F ' ' '{print $2}'`
    while [ ".$pid" != "." ]
    do
        echo 'Waiting for $1 to finish...'
        sleep 10s
        pid=`ps aux | grep $1 | grep -v grep | awk -F ' ' '{print $2}'`
    done
}

restart() {
    home="/home/jiamin"
    mysql_dir="${home}/mysql"
    db_base_dir="${home}/database"
    set_ld_path='export LD_LIBRARY_PATH=/home/jiamin/gcc/lib64:$LD_LIBRARY_PATH'
    stopmysql="${mysql_dir}/support-files/mysql stop"
    startmysql="${mysql_dir}/support-files/mysql start"
    reset_mysql="rm -rf ${mysql_dir} && cd ${home}/${sys}-5.6.23/build/5.6-release/ && make -j 64 install > make_out && rm -rf ${mysql_dir}/data"
    config_page="sed -r -i 's/innodb_page_size=[0-9]+k/innodb_page_size=${database}k/g' ${mysql_dir}/my.cnf"
    config_buf="sed -r -i 's/innodb_buffer_pool_size=[0-9]+G/innodb_buffer_pool_size=${buffer_size}G/g' ${mysql_dir}/my.cnf"
    config_flush="sed -r -i 's/innodb_flush_log_at_trx_commit=[0-9]+/innodb_flush_log_at_trx_commit=${flush}/g' ${mysql_dir}/my.cnf"
    config_schedule="sed -r -i 's/innodb_lock_schedule_algorithm=[0-9]+/innodb_lock_schedule_algorithm=${schedule}/g' ${mysql_dir}/my.cnf"
    reset_files="cp ${home}/my.cnf ${mysql_dir} && cp ${home}/my ${mysql_dir}/support-files/mysql && chmod +x ${mysql_dir}/support-files/mysql"

    if [[ $large_scale == true ]]; then
        config_page=""
    fi

    # $1 is rebuild
    if [[ $1 == true ]]; then
        rebuild="make -j 64 install -C $home/${sys}-5.6.23/build/5.6-release/&> make_out"
    else
        rebuild=""
    fi
    # $2 is reset database
    if [[ $2 == true ]]; then
        reset_db="echo 'Reseting database...' && cp -r ${db_base_dir}/${workload} ${mysql_dir}/data && echo 'Database reset done.'"
    else
        reset_db=""
    fi
    sleep10m="echo 'Sleep for $time' && sleep $time && echo 'Sleep done.'"
    taskset="ps aux | grep -v 'grep' | grep -v 'mysqld_safe' | grep mysqld | grep jiamin | awk -F ' ' 'FNR==1 {print \$2}' | xargs taskset -cp 0-1"

    ssh ${server} /home/jiamin/zsh/bin/zsh <<EOF
        $set_ld_path;
        $stopmysql;
        $rebuild;
        $reset_mysql;
        $reset_files;
        $config_buf;
        $config_flush;
        $config_schedule;
        $reset_db;
        $sleep10m;
        $startmysql;
        $taskset;
EOF
}

shutdown() {
    home="/home/jiamin"
    mysql_dir="${home}/mysql"
    db_base_dir="${home}/database"
    set_ld_path='export LD_LIBRARY_PATH=/home/jiamin/gcc/lib64:$LD_LIBRARY_PATH'
    stopmysql="${mysql_dir}/support-files/mysql stop"
    ssh ${server} /home/jiamin/zsh/bin/zsh <<EOF
    ${set_ld_path};
    ${stopmysql};
EOF
}

send_trx() {
    # $1 is name of experiment
    # $2 is the ordinal of the experiment
    # ssh salat2 /home/jiamin/zsh/bin/zsh <<EOF
    cd ~/oltpbench
    cp ~/oltp.xml ${server}.xml
    sed -r -i "s/#server/${server}/g" ${server}.xml 
    sed -r -i "s/#rate/${rate}/g" ${server}.xml 
    sed -r -i "s/#warehouse/${warehouse}/g" ${server}.xml
    # echo "~/STATsrv/bin/EasySTAT.sh ~/STATsrv/${schedule} 10 1.5"
    # ssh ${server} "~/STATsrv/bin/EasySTAT.sh ~/STATsrv/${schedule} 10 1.5&"
    echo "./oltpbenchmark -b ${workload} -c $config --execute=true -s 1 -o results/exp.$2"
    ./oltpbenchmark -b ${workload} -c $config --execute=true -s 1 -o results/exp.$2
    # sleep 10s
    # echo 'Sleep and then collect data'
    # ssh $server "cd ~/STATsrv/bin && sleep 21m && ./EasySTAT.sh ../$schedule 10 1"
    sleep 10s
# EOF
}

collect_result() {
    echo '**************************************'
    shutdown
    echo 'Collecting results to local machine...'
    # $1 is name of experiment
    # $2 is the ordinal of the experiment
    # $3 is the experiment directory
    scp -r ${server}:mysql/data/latency $3/latency$2
    scp ${server}:mysql/mysqld.log $3/latency$2
    # scp -r ${server}:mysql/data/enqueue $3/enqueue$2
    # scp -r ${server}:mysql/data/grant $3/grant$2
    # scp -r ${server}:mysql/data/work_wait /home/jiamin/work_wait/$1_$2
    # scp salat2:oltpbench/results/$1.$2.res $3/latency$2
    cp results/exp.$2.* $3/latency$2
    echo "Results copied to $3/latency$2"
    echo '**************************************'
}

run_experiment() {
    basedir="/home/jiamin/predictability/${sys}-5.6/${workload}"
    experiment_dir="$basedir/${experiment_name}"
    mkdir -p $experiment_dir
    rm_old_results $experiment_name
    for ((count = ${start}; count <= $end; count++))
    do
        echo '****************************************************************************'
        echo "Run experiment round $count"
        rm -rf $experiment_dir/latency$count
        rm -rf $experiment_dir/enqueue$count
        rm -rf $experiment_dir/grant$count
        echo 'Restarting MySQL...'
        # mkdir $experiment_dir/latency$count
        restart $do_rebuild $do_reset_db
        send_trx $experiment_name $count
        collect_result $experiment_name $count $experiment_dir
        echo "Experiment $count done."
        # build_linear_model $base_work_wait_dir/$experiment_name $count
    done
    echo '****************************************************************************'
}

for((rate = 600; rate <= 600; rate += 100))
do
    for schedule in fcfs
    do
        experiment_name="${schedule}_${suffix}"
        for workload in tpcc
        do
            #sed -i 's/mysqlname/username/g' oltpbench/${workload}.xml
            #sed -i 's/mysql>/password>/g' oltpbench/${workload}.xml
            database=${workload}
            config=${server}.xml
            run_experiment
        done
    done
done
