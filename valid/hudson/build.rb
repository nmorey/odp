#!/usr/bin/ruby

$LOAD_PATH.push('metabuild/lib')
require 'metabuild'
include Metabuild

options = Options.new({ "k1tools"       => [ENV["K1_TOOLCHAIN_DIR"].to_s,"Path to a valid compiler prefix."],
                        "artifacts"     => {"type" => "string", "default" => "", "help" => "Artifacts path given by Jenkins."},
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
build = ParallelTarget.new("build", repo, [])

$b = Builder.new("odp", options, [clean, build])

$b.logsession = "odp"

$b.default_targets = [build]

$current_target = options["target"]


$b.target("build") do
    $b.logtitle = "Report for odp build."
    cd odp_path
    
    $b.run(:cmd => "./syscall/run.sh", :env => $env)
    $b.run(:cmd => "./bootstrap", :env => $env)
    $b.run(:cmd => "CC=k1-nodeos-gcc  CXX=k1-nodeos-g++  ./configure  --host=k1-nodeos-magic -with-platform=k1-nodeos",
               :env => $env)
    $b.run(:cmd => "make", :env => $env)
end

$b.target("clean") do
    $b.logtitle = "Report for odp clean."

    cd odp_path
    $b.run(:cmd => "make clean", :env => $env)
    $b.run(:cmd => "make -Csyscall clean || true", :env => $env)
end


$b.launch

