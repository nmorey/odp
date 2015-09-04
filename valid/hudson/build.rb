#!/usr/bin/ruby
# coding: utf-8

$LOAD_PATH.push('metabuild/lib')
require 'metabuild'
include Metabuild
CONFIGS = `make list-configs`.split(" ").inject({}){|x, c| x.merge({ c => {} })}

options = Options.new({ "k1tools"       => [ENV["K1_TOOLCHAIN_DIR"].to_s,"Path to a valid compiler prefix."],
                        "debug"         => {"type" => "boolean", "default" => false, "help" => "Debug mode." },
                        "list-configs"  => {"type" => "boolean", "default" => false, "help" => "List all targets" },
                        "local-valid"   => {"type" => "boolean", "default" => false, "help" => "Valid using the local installation" },
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

local_valid = options["local-valid"]

clean = Target.new("clean", repo, [])
build = ParallelTarget.new("build", repo, [])
valid = ParallelTarget.new("valid", repo, [build])

install = Target.new("install", repo, [build])
if local_valid then
        long = Target.new("long", repo, [install])
else
        long = Target.new("long", repo, [])
end
package = Target.new("package", repo, [install])

b = Builder.new("odp", options, [clean, build, valid, long, package, install])

b.logsession = "odp"

b.default_targets = [valid]

debug_flags = options["debug"] == true ? "--enable-debug" : ""

valid_configs = options["valid-configs"].split()
valid_type = "sim"
if ENV["label"].to_s() != "" then
    case ENV["label"]
    when /MPPADevelopers-ab01b*/, /MPPAEthDevelopers-ab01b*/
        valid_configs = [ "k1b-kalray-nodeos", "k1b-kalray-mos" ]
        valid_type = "jtag"
    when /MPPADevelopers*/, /MPPAEthDevelopers*/
        valid_configs = [ "k1a-kalray-nodeos", "k1a-kalray-mos" ]
        valid_type = "jtag"
    when "fedora19-64","fedora17-64","debian6-64","debian7-64"
        # Validate nothing. It's centos7 job to do this
    when "centos7-64"
        valid_configs = [ "k1a-kalray-nodeos_simu", "k1a-kalray-mos_simu" ]
        valid_type = "sim"
    when /MPPAExplorers_k1b*/
        #valid_configs = [ "k1b-kalray-nodeos_explorer", "k1b-kalray-mos_explorer" ]
        valid_configs = [ "k1b-kalray-nodeos_explorer", "k1b-kalray-mos_explorer" ]
        valid_type = "jtag"
    else
        raise("Unsupported label #{ENV["label"]}!")
    end
end


configs = (options["configs"].split(" ")).uniq
configs.each(){|conf|
    raise ("Invalid config '#{conf}'") if CONFIGS[conf] == nil
}

if options["output-dir"] != nil then
    artifacts = File.expand_path(options["output-dir"])
else
    artifacts = File.join(workspace,"artifacts")
end
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

    if options['logtype'] == :junit then
        fName=File.dirname(options['logfile']) + "/" + "automake-tests.xml"
        b.valid(:cmd => "make junits CONFIGS='#{valid_configs.join(" ")}' JUNIT_FILE=#{fName}")
    end
end


b.target("long") do
    b.logtitle = "Report for odp tests."
    cd odp_path

    make_opt = ""
    if not local_valid then
        make_opt = "USE_PACKAGES=1"
    end

    b.run(:cmd => "make long #{make_opt} CONFIGS='#{valid_configs.join(" ")}'")

    valid_configs.each(){|conf|
        cd File.join(odp_path, "build", "long_" + conf, "bin")
        b.ctest( {
                     :ctest_args => "-L #{valid_type}",
                     :fail_msg => "Failed to validate #{conf}",
                     :success_msg => "Successfully validated #{conf}"
                 })
    }
end

b.target("install") do

    b.logtitle = "Report for odp install."
    cd odp_path

    b.run(:cmd => "rm -Rf install/")
    b.run(:cmd => "make install CONFIGS='#{configs.join(" ")}'")
end

b.target("package") do
    b.logtitle = "Report for odp package."
    cd odp_path

    b.run(:cmd => "cd install/; tar cf ../odp.tar local/k1tools/lib/ local/k1tools/share/odp/firmware local/k1tools/k1*/include local/k1tools/doc/ local/k1tools/lib64", :env => env)
    b.run(:cmd => "cd install/; tar cf ../odp-tests.tar local/k1tools/share/odp/*/tests", :env => env)
    b.run(:cmd => "cd cunit/install/; tar cf ../../odp-cunit.tar local/k1tools/kalray_internal", :env => env)

    (version,releaseID,sha1) = repo.describe()
    release_info = b.release_info(version,releaseID,sha1)
    sha1 = repo.sha1()

    #K1 ODP
    tar_package = File.expand_path("odp.tar")
    depends = []
    depends.push b.depends_info_struct.new("k1-tools","=", options["k1version"], "")
    package_description = "K1 ODP package (k1-odp-#{version}-#{releaseID} sha1 #{sha1})."
    pinfo = b.package_info("k1-odp", release_info,
                           package_description,
                           depends, "/usr", workspace)
    b.create_package(tar_package, pinfo)

    #K1 ODP Tests
    tar_package = File.expand_path("odp-tests.tar")
    depends = []
    depends.push b.depends_info_struct.new("k1-odp","=", release_info.full_version)
    package_description = "K1 ODP Standard Tests (k1-odp-tests-#{version}-#{releaseID} sha1 #{sha1})."
    pinfo = b.package_info("k1-odp-tests", release_info,
                           package_description, 
                           depends, "/usr", workspace)
    b.create_package(tar_package, pinfo)

    #K1 ODP Tests
    tar_package = File.expand_path("odp-cunit.tar")
    depends = []
    depends.push b.depends_info_struct.new("k1-odp-cunit","=", release_info.full_version)
    package_description = "K1 ODP CUnit (k1-odp-cunit-#{version}-#{releaseID} sha1 #{sha1})."
    pinfo = b.package_info("k1-odp-cunit", release_info,
                           package_description,
                           depends, "/usr", workspace)
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

