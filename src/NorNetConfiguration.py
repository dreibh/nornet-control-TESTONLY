#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# NorNet PLC Configuration
# Copyright (C) 2012-2013 by Thomas Dreibholz
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact: dreibh@simula.no

import re;
import sys;
import pwd;
import codecs;
#import getpass;


# XML-RPC
if sys.version_info < (3,0,0):
   import xmlrpclib;
else:
   import xmlrpc.client;

# Needs package python-ipaddr (Fedora Core, Ubuntu, Debian)!
from ipaddr import IPAddress, IPv4Address, IPv4Network, IPv6Address, IPv6Network;

# NorNet
from NorNetTools         import *;
from NorNetProviderSetup import *;



# ====== Adapt if necessary =================================================

NorNetPLC_ConstantsFile         = '/etc/nornet/nornetapi-constants'
NorNetPLC_FallbackConstantsFile = 'nornetapi-constants'

NorNetPLC_ConfigFile            = '/etc/nornet/nornetapi-config'
NorNetPLC_FallbackConfigFile    = 'nornetapi-config'

# These are the configuration defaults: just the parameters that need
# some setting in order to process the reading of the configuration from file.
NorNet_Configuration = {
   'NorNetPLC_Address'  : None,
   'NorNetPLC_User'     : 'nornetpp',
   'NorNetPLC_Password' : None,
   'NorNet_Provider0'   : '"UNKNOWN" "unknown"'
}

# ===========================================================================

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !!!! WARNING: Do not change unless you really know what you are doing! !!!!
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

# TOS Settings for provider selection
NorNet_TOSSettings = [ 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C ]

# Maximum number of external NTP servers
NorNet_MaxNTPServers = 8

# Maximum number of providers per site
NorNet_MaxProviders = 8

# NorNet Internet connection to/from outside world goes via Site 1!
NorNet_SiteIndex_Central = 1

# NorNet Tunnelbox is always Node 1!
NorNet_NodeIndex_Tunnelbox = 1

# PLC is Node 2 on the Central Site!
NorNet_SiteIndex_PLC = NorNet_SiteIndex_Central
NorNet_NodeIndex_PLC = 2

# NorNet Monitor is Node 3 on the Central Site!
NorNet_SiteIndex_Monitor  = NorNet_SiteIndex_Central
NorNet_NodeIndex_Monitor  = 3

# NorNet Monitor is Node 4 on the Central Site!
NorNet_SiteIndex_FileSrv  = NorNet_SiteIndex_Central
NorNet_NodeIndex_FileSrv  = 4

# ===========================================================================



# ###### Read configuration file ############################################
def loadNorNetConfiguration():
   log('Reading constants from ' + NorNetPLC_ConfigFile + ' ...')   
   try:
      constantsFile = codecs.open(NorNetPLC_ConstantsFile, 'r', 'utf-8')
   except:
      try:
         log('###### Cannot open ' + NorNetPLC_ConstantsFile + ', trying fallback file ' + NorNetPLC_FallbackConstantsFile + ' ... ######')
         constantsFile = codecs.open(NorNetPLC_FallbackConstantsFile, 'r', 'utf-8')

      except Exception as e:
         error('Constantsuration file ' + NorNetPLC_FallbackConstantsFile + ' cannot be read: ' + str(e))
   
   log('Reading configuration from ' + NorNetPLC_ConfigFile + ' ...')   
   try:
      configFile = codecs.open(NorNetPLC_ConfigFile, 'r', 'utf-8')
   except:
      try:
         log('###### Cannot open ' + NorNetPLC_ConfigFile + ', trying fallback file ' + NorNetPLC_FallbackConfigFile + ' ... ######')
         configFile = codecs.open(NorNetPLC_FallbackConfigFile, 'r', 'utf-8')

      except Exception as e:
         error('Configuration file ' + NorNetPLC_FallbackConfigFile + ' cannot be read: ' + str(e))

   lines = tuple(constantsFile) + tuple(configFile)
   for line in lines:
      if re.match('^[ \t]*[#\n]', line):
         continue
      elif re.match('^[a-zA-Z0-9_]*[ \t]*=', line):
         s = re.split('=',line,1)
         parameterName = s[0]
         parameterValue = unquote(removeComment(s[1].rstrip('\n')))
         NorNet_Configuration[parameterName] = parameterValue
         print '<' + parameterName + '> = <' + parameterValue + '>'
      else:
         error('Bad configuration line: ' + line)

   if NorNet_Configuration['NorNetPLC_Address'] == None:
      error('NorNetPLC_Address has not been set in configuration file!')
   if NorNet_Configuration['NorNetPLC_User'] == None:
      error('NorNetPLC_User has not been set in configuration file!')
   if NorNet_Configuration['NorNetPLC_Password'] == None:
      error('NorNetPLC_Password has not been set in configuration file!')
   try:
      user = pwd.getpwnam(getLocalNodeNorNetUser())
   except:
      error('NorNet_LocalNode_NorNetUser has invalid user "' + str(getLocalNodeNorNetUser()) + '"!')

   sys.stdout = codecs.getwriter('utf8')(sys.stdout)
   sys.stderr = codecs.getwriter('utf8')(sys.stderr)
   sys.stdin  = codecs.getreader('utf8')(sys.stdin)


# ###### Get local Site Index ###############################################
def getLocalSiteIndex():
   try:
      return int(NorNet_Configuration['NorNet_LocalSite_SiteIndex'])
   except:
      return None


# ###### Get local Default Provider Index ###################################
def getLocalDefaultProviderIndex():
   try:
      return int(NorNet_Configuration['NorNet_LocalSite_DefaultProviderIndex'])
   except:
      return None


# ###### Get local tunnelbox's outer IPv4 address ###########################
def getLocalTunnelboxDefaultProviderIPv4():
   return NorNet_Configuration['NorNet_LocalSite_TBDefaultProviderIPv4']


# ###### Get local node hostname ############################################
def getLocalNodeHostname():
   return NorNet_Configuration['NorNet_LocalNode_Hostname']


# ###### Get local node index ###############################################
def getLocalNodeIndex():
   try:
      return int(NorNet_Configuration['NorNet_LocalNode_Index'])
   except:
      return None


# ###### Get local node hostname ############################################
def getLocalNodeNorNetInterface():
   return NorNet_Configuration['NorNet_LocalNode_NorNetInterface']


# ###### Get local node NorNet user #########################################
def getLocalNodeNorNetUser():
   if NorNet_Configuration['NorNet_LocalNode_NorNetUser'] == None:
      return 'nornetpp'
   else:
      return NorNet_Configuration['NorNet_LocalNode_NorNetUser']


# ###### Get local node configuration string ################################
def getLocalNodeConfigurationString(nodeIndex):
   try:
      return unicode(NorNet_Configuration['NorNet_LocalSite_Node' + str(nodeIndex)])
   except:
      return u''


# ###### Get local node configuration string ################################
def getFileServRWSystemsConfigurationString():
   try:
      return NorNet_Configuration['NorNet_FileServ_RWSystems']
   except:
      return ''


# ###### Get DHCPD node configuration string ################################
def getLocalSiteDHCPServerDynamicConfigurationString():
   try:
      return NorNet_Configuration['NorNet_LocalSite_DHCPServer_Dynamic']
   except:
      return u''


# ###### Get DHCPD node configuration string ################################
def getLocalSiteDHCPServerStaticConfigurationString(nodeIndex):
   try:
      return unicode(NorNet_Configuration['NorNet_LocalSite_DHCPServer_Static' + str(nodeIndex)])
   except:
      return u''


# ###### Get NAT range ######################################################
def getLocalSiteNATRangeString():
   try:
      return unicode(NorNet_Configuration['NorNet_LocalSite_NAT_Range'])
   except:
      return u''
