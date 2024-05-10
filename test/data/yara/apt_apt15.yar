/*
   Yara Rule Set
   Author: Florian Roth
   Date: 2018-03-10
   Identifier: APT15 Report
   Reference: https://goo.gl/HZ5XMN
*/

/* Rule Set ----------------------------------------------------------------- */

import "pe"

rule APT15_Malware_Mar18_RoyalCli {
   meta:
      description = "Detects malware from APT 15 report by NCC Group"
      license = "Detection Rule License 1.1 https://github.com/Neo23x0/signature-base/blob/master/LICENSE"
      author = "Florian Roth (Nextron Systems)"
      reference = "https://goo.gl/HZ5XMN"
      date = "2018-03-10"
      hash1 = "6df9b712ff56009810c4000a0ad47e41b7a6183b69416251e060b5c80cd05785"
      id = "165bfa6c-1a8d-5628-8c35-da4e4a2ae04f"
   strings:
      $s1 = "\\Release\\RoyalCli.pdb" ascii
      $s2 = "%snewcmd.exe" fullword ascii
      $s3 = "Run cmd error %d" fullword ascii
      $s4 = "%s~clitemp%08x.ini" fullword ascii
      $s5 = "run file failed" fullword ascii
      $s6 = "Cmd timeout %d" fullword ascii
      $s7 = "2 %s  %d 0 %d" fullword ascii
   condition:
      uint16(0) == 0x5a4d and filesize < 200KB and 2 of them
}

rule APT15_Malware_Mar18_RoyalDNS {
   meta:
      description = "Detects malware from APT 15 report by NCC Group"
      license = "Detection Rule License 1.1 https://github.com/Neo23x0/signature-base/blob/master/LICENSE"
      author = "Florian Roth (Nextron Systems)"
      reference = "https://goo.gl/HZ5XMN"
      date = "2018-03-10"
      hash1 = "bc937f6e958b339f6925023bc2af375d669084e9551fd3753e501ef26e36b39d"
      id = "c2f519db-2750-53ce-ae18-697ea041faaf"
   strings:
      $x1 = "del c:\\windows\\temp\\r.exe /f /q" fullword ascii
      $x2 = "%s\\r.exe" fullword ascii

      $s1 = "rights.dll" fullword ascii
      $s2 = "\"%s\">>\"%s\"\\s.txt" fullword ascii
      $s3 = "Nwsapagent" fullword ascii
      $s4 = "%s\\r.bat" fullword ascii
      $s5 = "%s\\s.txt" fullword ascii
      $s6 = "runexe" fullword ascii
   condition:
      uint16(0) == 0x5a4d and filesize < 200KB and (
        ( pe.exports("RunInstallA") and pe.exports("RunUninstallA") ) or
        1 of ($x*) or
        2 of them
      )
}

rule APT15_Malware_Mar18_BS2005 {
   meta:
      description = "Detects malware from APT 15 report by NCC Group"
      license = "Detection Rule License 1.1 https://github.com/Neo23x0/signature-base/blob/master/LICENSE"
      author = "Florian Roth (Nextron Systems)"
      reference = "https://goo.gl/HZ5XMN"
      date = "2018-03-10"
      hash1 = "750d9eecd533f89b8aa13aeab173a1cf813b021b6824bc30e60f5db6fa7b950b"
      id = "700bbe14-d79e-5a35-aab3-31eacd5bd950"
   strings:
      $x1 = "AAAAKQAASCMAABi+AABnhEBj8vep7VRoAEPRWLweGc0/eiDrXGajJXRxbXsTXAcZAABK4QAAPWwAACzWAAByrg==" fullword ascii
      $x2 = "AAAAKQAASCMAABi+AABnhKv3kXJJousn5YzkjGF46eE3G8ZGse4B9uoqJo8Q2oF0AABK4QAAPWwAACzWAAByrg==" fullword ascii

      $a1 = "http://%s/content.html?id=%s" fullword ascii
      $a2 = "http://%s/main.php?ssid=%s" fullword ascii
      $a3 = "http://%s/webmail.php?id=%s" fullword ascii
      $a9 = "http://%s/error.html?tab=%s" fullword ascii

      $s1 = "%s\\~tmp.txt" fullword ascii
      $s2 = "%s /C %s >>\"%s\" 2>&1" fullword ascii
      $s3 = "DisableFirstRunCustomize" fullword ascii
   condition:
      uint16(0) == 0x5a4d and filesize < 200KB and (
         1 of ($x*) or
         2 of them
      )
}

rule APT15_Malware_Mar18_MSExchangeTool {
   meta:
      description = "Detects malware from APT 15 report by NCC Group"
      license = "Detection Rule License 1.1 https://github.com/Neo23x0/signature-base/blob/master/LICENSE"
      author = "Florian Roth (Nextron Systems)"
      reference = "https://goo.gl/HZ5XMN"
      date = "2018-03-10"
      hash1 = "16b868d1bef6be39f69b4e976595e7bd46b6c0595cf6bc482229dbb9e64f1bce"
      id = "81b826b6-8c2e-5a8a-a626-9515d40dbbb0"
   strings:
      $s1 = "\\Release\\EWSTEW.pdb" ascii
      $s2 = "EWSTEW.exe" fullword wide
      $s3 = "Microsoft.Exchange.WebServices.Data" fullword ascii
      $s4 = "tmp.dat" fullword wide
      $s6 = "/v or /t is null" fullword wide
   condition:
      uint16(0) == 0x5a4d and filesize < 40KB and all of them
}

/*
   Identifier: APT15 = Mirage = Ke3chang
   Author: NCCGroup
           Revised by Florian Roth for performance reasons
           see https://gist.github.com/Neo23x0/e3d4e316d7441d9143c7
           > some rules were untightened
   Date: 2018-03-09
   Reference: https://github.com/nccgroup/Royal_APT/blob/master/signatures/apt15.yara
*/

rule clean_apt15_patchedcmd{
   meta:
      author = "Ahmed Zaki"
      description = "This is a patched CMD. This is the CMD that RoyalCli uses."
      sha256 = "90d1f65cfa51da07e040e066d4409dc8a48c1ab451542c894a623bc75c14bf8f"
      id = "c6867ad4-f7f2-5d63-bffd-07599ede635d"
   strings:
      $ = "eisableCMD" wide
      $ = "%WINDOWS_COPYRIGHT%" wide
      $ = "Cmd.Exe" wide
      $ = "Windows Command Processor" wide
   condition:
      uint16(0) == 0x5A4D and all of them
}

rule malware_apt15_royalcli_1{
   meta:
      description = "Generic strings found in the Royal CLI tool"
      author = "David Cannings"
      sha256 = "6df9b712ff56009810c4000a0ad47e41b7a6183b69416251e060b5c80cd05785"
      id = "432c09bf-3c44-5a2c-ba69-7b4fe7eb43cc"
   strings:
      $ = "%s~clitemp%08x.tmp" fullword
      $ = "%s /c %s>%s" fullword
      $ = "%snewcmd.exe" fullword
      $ = "%shkcmd.exe" fullword
      $ = "%s~clitemp%08x.ini" fullword
      $ = "myRObject" fullword
      $ = "myWObject" fullword
      $ = "2 %s  %d 0 %d\x0D\x0A"
      $ = "2 %s  %d 1 %d\x0D\x0A"
      $ = "%s file not exist" fullword
   condition:
      uint16(0) == 0x5A4D and 5 of them
}

rule malware_apt15_royalcli_2{
   meta:
      author = "Nikolaos Pantazopoulos"
      description = "APT15 RoyalCli backdoor"
      id = "d4acfd2d-385d-5063-898e-d339b50733eb"
   strings:
      $string1 = "%shkcmd.exe" fullword
      $string2 = "myRObject" fullword
      $string3 = "%snewcmd.exe" fullword
      $string4 = "%s~clitemp%08x.tmp" fullword
      $string6 = "myWObject" fullword
   condition:
      uint16(0) == 0x5A4D and 2 of them
}


rule malware_apt15_bs2005{
   meta:
      author = "Ahmed Zaki"
      md5 = "ed21ce2beee56f0a0b1c5a62a80c128b"
      description = "APT15 bs2005"
   strings:
      $ = "%s&%s&%s&%s" wide ascii
      $ = "%s\\%s" wide ascii fullword
      $ = "WarOnPostRedirect"  wide ascii fullword
      $ = "WarnonZoneCrossing"  wide ascii fullword
      $ = "^^^^^" wide ascii fullword
      $ =  /"?%s\s*"?\s*\/C\s*"?%s\s*>\s*\\?"?%s\\(\w+\.\w+)?"\s*2>&1\s*"?/
      $ ="IEharden" wide ascii fullword
      $ ="DEPOff" wide ascii fullword
      $ ="ShownVerifyBalloon" wide ascii fullword
      $ ="IEHardenIENoWarn" wide ascii fullword
   condition:
      ( uint16(0) == 0x5A4D and 5 of them ) or
      ( uint16(0) == 0x5A4D and 3 of them and
            ( pe.imports("advapi32.dll", "CryptDecrypt") and pe.imports("advapi32.dll", "CryptEncrypt") and
              pe.imports("ole32.dll", "CoCreateInstance")
            )
      )
}


rule malware_apt15_royaldll_2 {
   meta:
      author = "Ahmed Zaki"
      sha256 = "bc937f6e958b339f6925023bc2af375d669084e9551fd3753e501ef26e36b39d"
      description = "DNS backdoor used by APT15"
      id = "3bc546a5-38b9-5504-b09e-305ba7bbd6bc"
   strings:
      $= "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Svchost" wide ascii
      $= "netsvcs" wide ascii fullword
      $= "%SystemRoot%\\System32\\svchost.exe -k netsvcs" wide ascii fullword
      $= "SYSTEM\\CurrentControlSet\\Services\\" wide ascii
      $= "myWObject" wide ascii
   condition:
      uint16(0) == 0x5A4D and all of them
      and pe.exports("ServiceMain")
      and filesize > 50KB and filesize < 600KB
}

rule malware_apt15_exchange_tool {
   meta:
      author = "Ahmed Zaki"
      md5 = "d21a7e349e796064ce10f2f6ede31c71"
      description = "This is a an exchange enumeration/hijacking tool used by an APT 15"
      id = "f07b9537-0741-51c8-a9fa-836430fe4855"
   strings:
      $s1= "subjectname" fullword
      $s2= "sendername" fullword
      $s3= "WebCredentials" fullword
      $s4= "ExchangeVersion" fullword
      $s5= "ExchangeCredentials" fullword
      $s6= "slfilename" fullword
      $s7= "EnumMail" fullword
      $s8= "EnumFolder" fullword
      $s9= "set_Credentials" fullword
      $s18 = "/v or /t is null" wide
      $s24 = "2013sp1" wide
   condition:
      uint16(0) == 0x5A4D and all of them
}
