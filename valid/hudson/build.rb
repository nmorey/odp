#!/usr/bin/ruby
# coding: utf-8

$LOAD_PATH.push('metabuild/lib')
require 'metabuild'
include Metabuild
CONFIGS={
	"k1a-kalray-nodeos" =>
    {
        :configure_options => "",
        :make_platform_options =>"",
        :make_test_options =>"",
        :platform => "k1-nodeos",
        :build_tests => true
    },
	"k1a-kalray-nodeosmagic" =>
    {
        :configure_options => "",
        :make_platform_options =>"",
        :make_test_options =>"",
        :platform => "k1-nodeos",
        :build_tests => true
    },
    "x86_64-unknown-linux-gnu" =>
    {
        :configure_options => "",
        :make_platform_options =>"",
        :make_test_options =>"",
        :platform => "linux-generic",
        :build_dirs => false
    }
	# "k1b-kalray-nodeos"      =>
    # {
    #     :configure_options => "",
    #     :make_platform_options =>"",
    #     :make_test_options =>"",
    #     :platform => "k1-nodeos"
    # },
	# "k1b-kalray-nodeosmagic" =>
    # {
    #     :configure_options => "",
    #     :make_platform_options =>"",
    #     :make_test_options =>"",
    #     :platform => "k1-nodeos"
    # },
}
$options = Options.new({ "k1tools"       => [ENV["K1_TOOLCHAIN_DIR"].to_s,"Path to a valid compiler prefix."],
                        "artifacts"     => {"type" => "string", "default" => "", "help" => "Artifacts path given by Jenkins."},
                        "debug"         => {"type" => "boolean", "default" => false, "help" => "Debug mode." },
                        "configs"       => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
                        "valid-configs" => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
                        "output-dir"    => [nil, "Output directory for RPMs."],
                        "k1version"     => ["unknown", "K1 Tools version required for building ODP applications"],
                     })

workspace  = $options["workspace"]
odp_clone  = $options['clone']
$odp_path   = File.join(workspace,odp_clone)

k1tools = $options["k1tools"]

$env = {}
$env["K1_TOOLCHAIN_DIR"] = k1tools
$env["PATH"] = "#{k1tools}/bin:#{ENV["PATH"]}"
$env["LD_LIBRARY_PATH"] = "#{k1tools}/lib:#{k1tools}/lib64:#{ENV["LD_LIBRARY_PATH"]}"

$repo = Git.new(odp_clone,workspace)


clean = Target.new("clean", $repo, [])
prep = ParallelTarget.new("prepare", $repo, [])
conf = ParallelTarget.new("configure", $repo, [prep])
build = ParallelTarget.new("build", $repo, [conf])
valid = ParallelTarget.new("valid", $repo, [build])
package = ParallelTarget.new("package", $repo, [build])

$b = Builder.new("odp", $options, [clean, prep, conf, build, valid, package])

$b.logsession = "odp"

$b.default_targets = [valid]

$debug_flags = $options["debug"] == true ? "--enable-debug" : ""

$valid_configs = $options["valid-configs"].split()

$configs = ($options["configs"].split(" ") + $valid_configs).uniq
$configs.each(){|conf|
    raise ("Invalid config '#{conf}'") if CONFIGS[conf] == nil
}

def conf_env(conf)
    arch = conf.split("-")[0]
    case arch
    when "x86_64"
        return ""
    when "k1a"
        return "CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ "
    else
        raise "Unsupported arch"
    end
end
$b.target("configure") do
    cd $odp_path
    $b.run(:cmd => "./bootstrap", :env => $env)
    $configs.each(){|conf|
        $b.run(:cmd => "rm -Rf build/#{conf}", :env => $env)
        $b.run(:cmd => "mkdir -p build/#{conf}", :env => $env)
        $b.run(:cmd => "cd build/#{conf}; #{conf_env(conf)}  #{$odp_path}/configure  --host=#{conf}" +
                       " --with-platform=#{CONFIGS[conf][:platform]}  " +
                       " --with-cunit-path=#{$odp_path}/cunit/install/#{conf}/ --enable-test-vald "+
                       " --prefix=#{$odp_path}/install/ --libdir=#{$odp_path}/install/lib/#{conf}" +
                       " --enable-test-perf #{$debug_flags} #{CONFIGS[conf][:configure_options]}",
           :env => $env)
    }
end

$b.target("prepare") do
    cd $odp_path
    $b.run(:cmd => "./syscall/run.sh", :env => $env)
    $b.run(:cmd => "./cunit/bootstrap", :env => $env)
    $configs.each(){|conf|
        $b.run(:cmd => "rm -Rf cunit/build/#{conf} cunit/install/#{conf}", :env => $env)
        $b.run(:cmd => "mkdir -p cunit/build/#{conf} cunit/install/#{conf}", :env => $env)
        $b.run(:cmd => "cd cunit/build/#{conf};  #{conf_env(conf)}  #{$odp_path}/cunit/configure --srcdir=#{$odp_path}/cunit "+
                       " --prefix=#{$odp_path}/cunit/install/#{conf}/ --enable-debug --enable-automated --enable-basic "+
                       " --enable-console --enable-examples --enable-test --host=#{conf}",
           :env => $env)
        $b.run(:cmd => "cd cunit/build/#{conf}; make -j4 install V=1", :env => $env)
    }
end

$b.target("build") do
    $b.logtitle = "Report for odp build."
    cd $odp_path

     $configs.each(){|conf|
        $b.run(:cmd => "make -Cbuild/#{conf}/platform #{CONFIGS[conf][:make_platform_options]} V=1 install", :env => $env)
        if CONFIGS[conf][:build_tests] then
            $b.run(:cmd => "make -Cbuild/#{conf}/test #{CONFIGS[conf][:make_test_options]} V=1" , :env => $env)
        end
        $b.run(:cmd => "make -Cbuild/#{conf}/example/generator", :env => $env)
    }
end

$b.target("valid") do
    $b.logtitle = "Report for odp tests."
    cd $odp_path

     $valid_configs.each(){|conf|
        $b.valid(:cmd => "make -Cbuild/#{conf}/test/validation -j1 check", :env => $env)
        $b.valid(:cmd => "make -Cbuild/#{conf}/test/performance -j1 check", :env => $env)
     }
end

$b.target("package") do
    $b.logtitle = "Report for odp tests."
    cd $odp_path

    $b.run(:cmd => "cd install/; tar cf ../odp.tar lib/ include", :env => $env)
    tar_package = File.expand_path("odp.tar")

    depends = []
    depends.push $b.depends_info_struct.new("k1-dev","=", $options["k1version"], "")

    (version,releaseID,sha1) = $repo.describe()
    release_info = $b.release_info(version,releaseID,sha1)

    sha1 = $repo.sha1()
    package_description = "K1 ODP package (k1-odp-#{version}-#{releaseID} sha1 #{sha1})."
    pinfo = $b.package_info("k1-odp", release_info,
                           package_description, "/usr/local/k1tools/k1-nodeos",
                           workspace, depends)
    if $options["output-dir"] != nil then
            $b.run(:cmd => "mkdir -p #{$options["output-dir"]}", :env => $env)
            pinfo.output_dir = $options["output-dir"]
    end
    $b.create_package(tar_package, pinfo)
end

$b.target("clean") do
    $b.logtitle = "Report for odp clean."

    cd $odp_path
    $configs.each(){|conf|
        $b.run(:cmd => "rm -Rf build/#{conf}", :env => $env)
        $b.run(:cmd => "rm -Rf cunit/build/#{conf} cunit/install/#{conf}", :env => $env)
    }
end


$b.launch

