#!/usr/bin/ruby
# coding: utf-8

$LOAD_PATH.push('metabuild/lib')
require 'metabuild'
include Metabuild
CONFIGS = `make list-configs`.split(" ").inject({}){|x, c| x.merge({ c => {} })}

$options = Options.new({ "k1tools"       => [ENV["K1_TOOLCHAIN_DIR"].to_s,"Path to a valid compiler prefix."],
                        "artifacts"     => {"type" => "string", "default" => "", "help" => "Artifacts path given by Jenkins."},
                        "debug"         => {"type" => "boolean", "default" => false, "help" => "Debug mode." },
                        "list-configs"  => {"type" => "boolean", "default" => false, "help" => "List all targets" },
                        "configs"       => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
                        "valid-configs" => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
                        "output-dir"    => [nil, "Output directory for RPMs."],
                        "k1version"     => ["unknown", "K1 Tools version required for building ODP applications"],
                     })

if $options["list-configs"] == true then
    puts CONFIGS.map(){|n, i| n}.join("\n")
    exit 0
end
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
build = ParallelTarget.new("build", $repo, [])
valid = ParallelTarget.new("valid", $repo, [build])
package = ParallelTarget.new("package", $repo, [build])

$b = Builder.new("odp", $options, [clean, build, valid, package])

$b.logsession = "odp"

$b.default_targets = [valid]

$debug_flags = $options["debug"] == true ? "--enable-debug" : ""

$valid_configs = $options["valid-configs"].split()

$configs = ($options["configs"].split(" ")).uniq
$configs.each(){|conf|
    raise ("Invalid config '#{conf}'") if CONFIGS[conf] == nil
}

def conf_env(conf)
    arch = conf.split("-")[0]
    case arch
    when "x86_64"
        return ""
    when "k1a","k1b"
        return "CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ "
    else
        raise "Unsupported arch"
    end
end

$b.target("build") do
    $b.logtitle = "Report for odp build."
    cd $odp_path
    $b.run(:cmd => "make -j4 build CONFIGS='#{$configs.join(" ")}'")
end

$b.target("valid") do
    $b.logtitle = "Report for odp tests."
    cd $odp_path

    $b.run(:cmd => "make -j4 valid CONFIGS='#{$valid_configs.join(" ")}'")
end

$b.target("package") do
    $b.logtitle = "Report for odp tests."
    cd $odp_path
    $b.run(:cmd => "rm -Rf install/")
    $b.run(:cmd => "make -j1 install CONFIGS='#{$configs.join(" ")}'")

    $b.run(:cmd => "cd install/; tar cf ../odp.tar local/k1tools/lib/ local/k1tools/k1*/include local/k1tools/doc/ local/k1tools/lib64", :env => $env)
    tar_package = File.expand_path("odp.tar")

    depends = []
    depends.push $b.depends_info_struct.new("k1-tools","=", $options["k1version"], "")

    (version,releaseID,sha1) = $repo.describe()
    release_info = $b.release_info(version,releaseID,sha1)

    sha1 = $repo.sha1()
    package_description = "K1 ODP package (k1-odp-#{version}-#{releaseID} sha1 #{sha1})."
    pinfo = $b.package_info("k1-odp", release_info,
                           package_description, "/usr",
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
    $b.run(:cmd => "make clean", :env => $env)
end


$b.launch

