from Page import *
from Table import *
from configured import *

import xmlrpclib

URL = 'http://www.octality.com/api/'

def Octality_Proud_SendDomains (cfg):
    domains = []

    def valid_domain_name(txt):
        if '.' in txt:
            return True
        if ':' in txt:
            return True

    # Collect nick names
    for v in cfg.keys('vserver'):
        nick = cfg.get_val ('vserver!%s!nick'%(v))
        if valid_domain_name(nick):
            domains.append (nick)

        for d in cfg.keys('vserver!%s!match!domain'%(v)):
            domain = cfg.get_val('vserver!%s!match!domain!%s'%(v,d))
            domains.append (domain)

        for d in cfg.keys('vserver!%s!match!regex'%(v)):
            domain = cfg.get_val('vserver!%s!match!regex!%s'%(v,d))
            domains.append (domain)

    # Send them
    xmlrpc = xmlrpclib.ServerProxy (URL)
    return xmlrpc.proud_users.add_domains (domains)


class PageOWS (PageMenu, FormHelper):
    def __init__ (self, cfg):
        PageMenu.__init__ (self, 'OWS', cfg)
        FormHelper.__init__ (self, 'OWS', cfg)

    def _op_handler (self, uri, post):
        try:
            if uri == '/proud_submit_domains':
                return self._proud_submit_domains()
            else:
                print "ERROR: Unknown URL '%s'" %(uri)
                return '/'
        except Exception, e:
            print "Something REALLY bad happened", str(e)

    def _proud_submit_domains (self):
        txt = ''

        ret = Octality_Proud_SendDomains (self._cfg)
        for domain in ret:
            txt += '<b>%s</b> - %s<br/>' % (domain, ret[domain])

        return txt

