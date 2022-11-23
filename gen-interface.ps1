param(
    [string]$ThriftExecutable="/opt/vesoft/third-party/2.0/bin/thrift1.exe",
    [bool]$SyncRemoteInterface=$false
) 

$NEBULA_INTERFACE_HOME="$PSScriptRoot/src/interface"
Set-Location $NEBULA_INTERFACE_HOME
$modes = 'common', 'meta', 'storage', 'graph'
foreach ($mod in $modes) {
    if ($SyncRemoteInterface) {
        Invoke-WebRequest -O $NEBULA_INTERFACE_HOME/$mod.thrift https://raw.githubusercontent.com/vesoft-inc/nebula/master/src/interface/$mod.thrift
    }
    "& $ThriftExecutable --strict --allow-neg-enum-vals --gen `"mstch_cpp2:include_prefix=`${include_prefix},stack_arguments`" -o $NEBULA_INTERFACE_HOME $mod.thrift"
    Invoke-Expression "& $ThriftExecutable --strict --allow-neg-enum-vals --gen `"mstch_cpp2:stack_arguments`" -o $NEBULA_INTERFACE_HOME $mod.thrift"
}
Set-Location $PSScriptRoot