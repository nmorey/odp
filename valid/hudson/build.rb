#!/usr/bin/ruby

$LOAD_PATH.push('metabuild/lib')
require 'metabuild'
include Metabuild

options = Options.new({ "k1tools"       => [ENV["K1_TOOLCHAIN_DIR"].to_s,"Path to a valid compiler prefix."],
                        "artifacts"     => {"type" => "string", "default" => "", "help" => "Artifacts path given by Jenkins."},
                        "debug"         => {"type" => "boolean", "default" => false, "help" => "Debug mode." },
                      })

workspace  = options["workspace"]
odp_clone  = options['clone']
odp_path   = File.join(workspace,odp_clone)

k1tools = options["k1tools"]

$env = {}
$env["K1_TOOLCHAIN_DIR"] = k1tools
$env["PATH"] = "#{k1tools}/bin:#{ENV["PATH"]}"
$env["LD_LIBRARY_PATH"] = "#{k1tools}/lib:#{k1tools}/lib64:#{ENV["LD_LIBRARY_PATH"]}"

repo = Git.new(odp_clone,workspace)

clean = Target.new("clean", repo, [])
prep = ParallelTarget.new("prepare", repo, [])
conf = ParallelTarget.new("configure", repo, [prep])
build = ParallelTarget.new("build", repo, [conf])
valid = ParallelTarget.new("valid", repo, [build])

$b = Builder.new("odp", options, [clean, prep, conf, build, valid])

$b.logsession = "odp"

$b.default_targets = [build]

$current_target = options["target"]
$debug_flags = options["debug"] == true ? "--enable-debug" : ""

$b.target("configure") do
    cd odp_path
    $b.run(:cmd => "./bootstrap", :env => $env)
    $b.run(:cmd => "rm -Rf build-k1-nodeos build-k1-nodeos-magic", :env => $env)
    $b.run(:cmd => "mkdir -p build-k1-nodeos build-k1-nodeos-magic", :env => $env)
    $b.run(:cmd => "cd build-k1-nodeos; CC=k1-nodeos-gcc  CXX=k1-nodeos-g++  ../configure  --host=k1-nodeos -with-platform=k1-nodeos  --with-cunit-path=$(pwd)/../cunit/build/ --enable-test-vald --enable-test-perf #{$debug_flags} ",
           :env => $env)
   $b.run(:cmd => "cd build-k1-nodeos-magic; CC=k1-nodeos-gcc  CXX=k1-nodeos-g++  ../configure  --host=k1-nodeos-magic -with-platform=k1-nodeos  --with-cunit-path=$(pwd)/../cunit/build/ --enable-test-vald --enable-test-perf #{$debug_flags} ",
               :env => $env)
end

$b.target("prepare") do
    cd odp_path
    $b.run(:cmd => "./syscall/run.sh", :env => $env)
    $b.run(:cmd => "./cunit/bootstrap", :env => $env)
end

$b.target("build") do
    $b.logtitle = "Report for odp build."
    cd odp_path

    ["k1-nodeos", "k1-nodeos-magic"].each(){|platform|
        $b.run(:cmd => "make -Cbuild-#{platform}/platform V=1", :env => $env)
        $b.run(:cmd => "make -Cbuild-#{platform}/test", :env => $env)
        $b.run(:cmd => "make -Cbuild-#{platform}/test/validation", :env => $env)
        $b.run(:cmd => "make -Cbuild-#{platform}/example/generator", :env => $env)
    }
end

$b.target("valid") do
    $b.logtitle = "Report for odp tests."
    cd odp_path

    $b.run(:cmd => " k1-cluster --mboard=large_memory --functional --user-syscall=syscall/build_x86_64/libuser_syscall.so -- build-k1-nodeos-magic/test/performance/odp_atomic -t 1 -n 15 ", :env => $env)
end

$b.target("clean") do
    $b.logtitle = "Report for odp clean."

    cd odp_path
    $b.run(:cmd => "make clean", :env => $env)
    $b.run(:cmd => "make -Csyscall clean || true", :env => $env)
end


$b.launch

