#!/usr/bin/ruby
# coding: utf-8

$LOAD_PATH.push('metabuild/lib')
require 'metabuild'
include Metabuild
CONFIGS={
	"k1a-kalray-nodeos" =>
    {
        :configure_options => " --disable-crypto",
        :make_platform_options =>"",
        :make_test_options =>""
    },
	"k1a-kalray-nodeosmagic" =>
    {
        :configure_options => " --disable-crypto",
        :make_platform_options =>"",
        :make_test_options =>""
    },
	"k1b-kalray-nodeos"      =>
    {
        :configure_options => " --disable-crypto",
        :make_platform_options =>"",
        :make_test_options =>""
    },
	"k1b-kalray-nodeosmagic" =>
    {
        :configure_options => " --disable-crypto",
        :make_platform_options =>"",
        :make_test_options =>""
    },
}
options = Options.new({ "k1tools"       => [ENV["K1_TOOLCHAIN_DIR"].to_s,"Path to a valid compiler prefix."],
                        "artifacts"     => {"type" => "string", "default" => "", "help" => "Artifacts path given by Jenkins."},
                        "debug"         => {"type" => "boolean", "default" => false, "help" => "Debug mode." },
                        "configs"       => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
                        "valid-configs" => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
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

$b.default_targets = [valid]

$debug_flags = options["debug"] == true ? "--enable-debug" : ""

$valid_configs = options["valid-configs"].split()

$configs = (options["configs"].split(" ") + $valid_configs).uniq
$configs.each(){|conf|
    raise ("Invalid config '#{conf}'") if CONFIGS[conf] == nil
}

def conf_env(conf)
    arch = conf.split("-")[0]
    return "CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ CFLAGS='-march=#{arch}' CPPFLAGS='-march=#{arch}' " +
           CONFIGS[conf][:configure_env].to_s + " "
end
$b.target("configure") do
    cd odp_path
    $b.run(:cmd => "./bootstrap", :env => $env)
    $configs.each(){|conf|
        $b.run(:cmd => "rm -Rf build-#{conf}", :env => $env)
        $b.run(:cmd => "mkdir -p build-#{conf}", :env => $env)
        $b.run(:cmd => "cd build-#{conf}; #{conf_env(conf)}  ../configure  --host=#{conf}" +
                       " -with-platform=k1-nodeos  --with-cunit-path=$(pwd)/../cunit/install-#{conf}/ --enable-test-vald "+
                       " --enable-test-perf #{$debug_flags} #{CONFIGS[conf][:configure_options]}",
           :env => $env)
    }
end

$b.target("prepare") do
    cd odp_path
    $b.run(:cmd => "./syscall/run.sh", :env => $env)
    $b.run(:cmd => "./cunit/bootstrap", :env => $env)
    $configs.each(){|conf|
        $b.run(:cmd => "rm -Rf cunit/build-#{conf} cunit/install-#{conf}", :env => $env)
        $b.run(:cmd => "mkdir -p cunit/build-#{conf} cunit/install-#{conf}", :env => $env)
        $b.run(:cmd => "cd cunit/build-#{conf};  #{conf_env(conf)}  ../configure --srcdir=`pwd`/.."+
                       " --prefix=$(pwd)/../install-#{conf}/ --enable-debug --enable-automated --enable-basic "+
                       " --enable-console --enable-examples --enable-test --host=#{conf}",
           :env => $env)
        $b.run(:cmd => "cd cunit/build-#{conf}; make -j4 install", :env => $env)
    }
end

$b.target("build") do
    $b.logtitle = "Report for odp build."
    cd odp_path

     $configs.each(){|conf|
        $b.run(:cmd => "make -Cbuild-#{conf}/platform #{CONFIGS[conf][:make_platform_options]} V=1", :env => $env)
        $b.run(:cmd => "make -Cbuild-#{conf}/test #{CONFIGS[conf][:make_test_options]} V=1" , :env => $env)
        $b.run(:cmd => "make -Cbuild-#{conf}/example/generator", :env => $env)
    }
end

$b.target("valid") do
    $b.logtitle = "Report for odp tests."
    cd odp_path

     $valid_configs.each(){|conf|
        $b.run(:cmd => "make -Cbuild-#{conf}/test/validation -j1 check", :env => $env)
        $b.run(:cmd => "make -Cbuild-#{conf}/test/performance -j1 check", :env => $env)
     }
end

$b.target("clean") do
    $b.logtitle = "Report for odp clean."

    cd odp_path
    $configs.each(){|conf|
        $b.run(:cmd => "rm -Rf build-#{conf}", :env => $env)
        $b.run(:cmd => "rm -Rf cunit/build-#{conf} cunit/install-#{conf}", :env => $env)
    }
end


$b.launch

