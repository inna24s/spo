<?php
parse_str(implode('&', array_slice($argv, 1)), $_GET);
if(isset($_GET["path"])){
	echo 
    shell_mode($_GET["path"]);
}
else{
    echo "Wrong arguments\n";
	echo "Enter arguments!\n";
    echo "keys:\n";
    echo "path=<path> (to work with HFS+: path=hfsimg.bin)\n";
}


function shell_mode($path){
    $end_flag = false;
	#создание библиотеки
    $hfs_library = FFI::cdef(file_get_contents("./hfs_task/utils.h"), "./libhfsplus_utils.so");
    $filesystem = $hfs_library->getHFSPlus($path);
    if($filesystem == null){
        echo "Error! This is not exist or does not supports HFS+ filesystem!";
        return;
    }
    while (!$end_flag) {
        $line = readline(">");
        if ($line == "") {
            continue;
        }
        $command = explode(" ", $line);
        switch ($command[0]) {
            case "exit":
            {
                $end_flag = true;
                break;
            }
            case "help":
            {
                help();
                break;
            }
            case "ls":
            {
                ls($command, $hfs_library, $filesystem);
                break;
            }
            case "pwd":
            {
                pwd($hfs_library, $filesystem);
                break;
            }
            case "cd":
            {
                cd($command, $hfs_library, $filesystem);
                break;
            }
            case "cp":
            {
                cp($command, $hfs_library, $filesystem);
                break;
            }
            default:
            {
                echo "Wrong command. Use 'help' command to get help\n";
            }
        }
    }
    $hfs_library->freeHFSPlus($filesystem);
}

function help(){
    echo "help\n";
}

function ls($command, $lib, $filesystem){
	$path = $command[1];
	$answer = $lib->ls($filesystem, $path);
	#Создаёт строку PHP из области памяти
	echo FFI::string($answer);
	FFI::free($answer);
}

function pwd($lib, $filesystem){
    $answer = $lib->f_pwd($filesystem);
    echo FFI::string($answer);
    FFI::free($answer);
}

function cd($command, $lib, $filesystem){
	$path = $command[1];
	$answer = $lib->cd($filesystem, $path);
	echo FFI::string($answer);
	FFI::free($answer);
}


function cp($command, $lib, $filesystem){
    $num_args = count($command);
    if($num_args < 2){
        echo "Path is not specified\n";
    }
    else if($num_args < 3){
        echo "Output path is not specified\n";
    }
    else{
        $input_path = $command[1];
        $out_path = $command[2];
        $answer = $lib->cp($filesystem, $input_path, $out_path);
        echo FFI::string($answer);
        FFI::free($answer);
    }
}
