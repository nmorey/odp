#!/usr/bin/ruby
# coding: utf-8

$LOAD_PATH.push('metabuild/lib')
require 'metabuild'
include Metabuild
CONFIGS = `make list-configs`.split(" ").inject({}){|x, c| x.merge({ c => {} })}

options = Options.new({ "k1tools"       => [ENV["K1_TOOLCHAIN_DIR"].to_s,"Path to a valid compiler prefix."],
                        "artifacts"     => {"type" => "string", "default" => "", "help" => "Artifacts path given by Jenkins."},
                        "debug"         => {"type" => "boolean", "default" => false, "help" => "Debug mode." },
                        "list-configs"  => {"type" => "boolean", "default" => false, "help" => "List all targets" },
                        "configs"       => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
                        "valid-configs" => {"type" => "string", "default" => CONFIGS.keys.join(" "), "help" => "Build configs. Default = #{CONFIGS.keys.join(" ")}" },
                        "output-dir"    => [nil, "Output directory for RPMs."],
                        "k1version"     => ["unknown", "K1 Tools version required for building ODP applications"],
                     })

if options["list-configs"] == true then
    puts CONFIGS.map(){|n, i| n}.join("\n")
    exit 0
end
workspace  = options["workspace"]
odp_clone  = options['clone']
jobs = options['jobs']

odp_path   = File.join(workspace,odp_clone)

k1tools = options["k1tools"]

env = {}
env["K1_TOOLCHAIN_DIR"] = k1tools
env["PATH"] = "#{k1tools}/bin:#{ENV["PATH"]}"
env["LD_LIBRARY_PATH"] = "#{k1tools}/lib:#{k1tools}/lib64:#{ENV["LD_LIBRARY_PATH"]}"

repo = Git.new(odp_clone,workspace)


clean = Target.new("clean", repo, [])
build = ParallelTarget.new("build", repo, [])
valid = ParallelTarget.new("valid", repo, [build])
long = ParallelTarget.new("long", repo, [])
package = Target.new("package", repo, [build])

b = Builder.new("odp", options, [clean, build, valid, long, package])

b.logsession = "odp"

b.default_targets = [valid]

debug_flags = options["debug"] == true ? "--enable-debug" : ""

valid_configs = options["valid-configs"].split()

configs = (options["configs"].split(" ")).uniq
configs.each(){|conf|
    raise ("Invalid config '#{conf}'") if CONFIGS[conf] == nil
}
artifacts = File.expand_path(options["artifacts"])
artifacts = File.join(workspace,"artifacts") if(options["artifacts"].empty?)
mkdir_p artifacts unless File.exists?(artifacts)

b.target("build") do
    b.logtitle = "Report for odp build."
    cd odp_path
    b.run(:cmd => "make build CONFIGS='#{configs.join(" ")}'")
end

b.target("valid") do
    b.logtitle = "Report for odp tests."
    cd odp_path

    b.valid(:cmd => "make valid CONFIGS='#{valid_configs.join(" ")}'")
end


b.target("long") do
    b.logtitle = "Report for odp tests."
    cd odp_path

    b.valid(:cmd => "make -Clong all CONFIGS='#{valid_configs.join(" ")}'")
end

b.target("package") do
    b.logtitle = "Report for odp tests."
    cd odp_path
    b.run(:cmd => "rm -Rf install/")
    b.run(:cmd => "make install CONFIGS='#{configs.join(" ")}'")

    b.run(:cmd => "cd install/; tar cf ../odp.tar local/k1tools/lib/ local/k1tools/k1*/include local/k1tools/doc/ local/k1tools/lib64", :env => env)
    b.run(:cmd => "cd install/; tar cf ../odp-tests.tar local/k1tools/share/odp/tests", :env => env)

    (version,releaseID,sha1) = repo.describe()
    release_info = b.release_info(version,releaseID,sha1)
    sha1 = repo.sha1()

    #K1 ODP
    tar_package = File.expand_path("odp.tar")
    depends = []
    depends.push b.depends_info_struct.new("k1-tools","=", options["k1version"], "")
    package_description = "K1 ODP package (k1-odp-#{version}-#{releaseID} sha1 #{sha1})."
    pinfo = b.package_info("k1-odp", release_info,
                           package_description, "/usr",
                           workspace, depends)
    b.create_package(tar_package, pinfo)

    #K1 ODP Tests
    tar_package = File.expand_path("odp-tests.tar")
    depends = []
    depends.push b.depends_info_struct.new("k1-odp","=", release_info.full_version)
    package_description = "K1 ODP Standard Tests (k1-odp-tests-#{version}-#{releaseID} sha1 #{sha1})."
    pinfo = b.package_info("k1-odp-tests", release_info,
                           package_description, "/usr",
                           workspace, depends)
    b.create_package(tar_package, pinfo)


  # Generates k1r_parameters.sh
    output_parameters = File.join(artifacts,"k1odp_parameters.sh")
  b.run("rm -f #{output_parameters}")
  b.run("echo 'K1ODP_VERSION=#{$version}-#{$buildID}' >> #{output_parameters}")
  b.run("echo 'K1ODP_RELEASE=#{$version}' >> #{output_parameters}")
  b.run("echo 'COMMITER_EMAIL=#{options["email"]}' >> #{output_parameters}")
  b.run("echo 'INTEGRATION_BRANCH=#{ENV.fetch("INTEGRATION_BRANCH",options["branch"])}' >> #{output_parameters}")
  b.run("echo 'REVISION=#{repo.long_sha1()}' >> #{output_parameters}")
  b.run("#{workspace}/metabuild/bin/packages.rb --tar=#{File.join(artifacts,"package.tar")} tar")

end

b.target("clean") do
    b.logtitle = "Report for odp clean."

    cd odp_path
    b.run(:cmd => "make clean", :env => env)
end


b.launch

