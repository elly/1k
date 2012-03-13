#!/usr/bin/env python
# irc.py - IRC protocol library

import random
import sys

"""
irc - IRC protocol library

Unlike most IRC protocol implementations, this library is solely concerned with
speaking the IRC protocol; it does not do any networking of its own. This module
provides five classes: "IRC", "Event", "Channel", "User", and "Server".

The IRC class maintains a single session's IRC state, and consumes input lines
to produce output lines and events; the Event class represents a single event.
Channel, User, and Server represent entities in the IRC protocol.
"""

class ProtocolError(Exception):
    pass

class Event:
    """
    Represents a single IRC event.
    """
    def __init__(self, kind, src, dest=None, target=None, args=None): # {{{
        self._kind = kind
        self._src = src
        self._dest = dest
        self._target = target
        self._args = args
    # }}}
    def kind(self): # {{{
        return self._kind
    # }}}
    def src(self): # {{{
        return self._src
    # }}}
    def dest(self): # {{{
        return self._dest
    # }}}
    def target(self): # {{{
        return self._target
    # }}}
    def args(self): # {{{
        return self._args
    # }}}
    def __str__(self): # {{{
        return '<Event kind=%s src=%s dest=%s target=%s args=%s>' % (
            self._kind, self._src, self._dest, self._target, self._args)
    # }}}

class Channel:
    """
    Represents a single IRC channel.
    """
    def _add_user(self, user, modes=''): # {{{
        self._users[user] = modes
    # }}}
    def __init__(self, name): # {{{
        self._name = name
        self._users = {}
    # }}}
    def name(self): # {{{
        return self._name
    # }}}
    def is_user(self): # {{{
        return False
    # }}}
    def is_server(self): # {{{
        return False
    # }}}
    def is_channel(self): # {{{
        return True
    # }}}
    def __str__(self): # {{{
        return '<Channel "%s">' % self._name
    # }}}
    def users(self): # {{{
        return _users
    # }}}

class User:
    """
    Represents a single IRC user.
    """
    def _update(self, user, host): # {{{
        self._user = user
        self._host = host
    # }}}
    def __init__(self, nick, user, host): # {{{
        self._nick = nick
        self._user = user
        self._host = host
    # }}}
    def nick(self): # {{{
        return self._nick
    # }}}
    def user(self): # {{{
        return self._user
    # }}}
    def host(self): # {{{
        return self._host
    # }}}
    def is_user(self): # {{{
        return True
    # }}}
    def is_server(self): # {{{
        return False
    # }}}
    def is_channel(self): # {{{
        return False
    # }}}
    def __str__(self): # {{{
        return '<User nick="%s">' % self._nick
    # }}}

class Server:
    """
    Represents a single IRC server.
    """
    def __init__(self, name): # {{{
        self._name = name
    # }}}
    def name(self): # {{{
        return self._name
    # }}}
    def is_user(self): # {{{
        return False
    # }}}
    def is_server(self): # {{{
        return True
    # }}}
    def is_channel(self): # {{{
        return False
    # }}}
    def __str__(self): # {{{
        return '<Server "%s">' % self._name
    # }}}

class IRC:
    """
    Represents a single IRC client session. Fed input lines; produces output
    lines (to be sent to the server) and events. Other methods can be used to
    send commands to the IRC server, which will appear in the output line
    buffer.
    """
    def _genstr(self, template): # {{{
        while 'X' in template:
            chars = 'abcdefghijklmnopqrstuvwxyz0123456789'
            c = self._random.choice(chars)
            template = template.replace('X', c, 1)
            print template
        return template
    # }}}
    def _add_user(self, user): # {{{
        assert(user.nick() not in self._users)
        self._users[user.nick()] = user
    # }}}
    def _parsesource(self, src): # {{{
        if src[0] == ':':
            src = src[1:]

        if '!' in src and '@' in src:
            # Long-form nick!user@host
            (nick, userathost) = src.split('!', 1)
            (user, host) = userathost.split('@', 1)
            u = self.user(nick)
            if not u:
                u = User(nick, user, host)
                # Note that we don't add the new user to our dictionary - we
                # can't necessarily see them, and if we can't see users, we
                # don't know when to clean out the old reference to them.
                # Messages that indicate that we can see users (channel joins)
                # add the source themselves if need be.
            if u.user() != user or u.host() != host:
                # If we see a user@host that doesn't match the existing
                # user@host for an old user, update it here.
                u._update(user, host)
            return u

        if '!' not in src and '@' not in src and '.' in src:
            # Dotted server name
            s = self.server(src)
            if not s:
                s = Server(src)
            return s

        # Short-form user reference (just nick). Never actually seen in the
        # wild, but the RFC allows it...
        u = self.user(src)
        if not u:
            u = User(src, None, None)
        return u
    # }}}
    def _parsedest(self, dest): # {{{
        if dest[0] in self._opts['chantypes']:
            c = self.channel(dest)
            if not c:
                c = Channel(dest)
                # Don't learn about c here. We might be seeing an event for a
                # channel we're not in (!?), although that'd be pretty messed
                # up.
            return c
        elif dest == '*':
            # Special case - some ircds send this as the dest of pre-auth
            # notices. Not a valid nickname, in any case.
            return None
        else:
            u = self.user(dest)
            if not u:
                u = User(dest, None, None)
            return u
    # }}}
    def _parse_ping(self, line): # {{{
        (cmd, rest) = line.split(' ', 1)
        self._add_output('PONG %s' % rest)
    # }}}
    def _parse_001(self, src, cmd, rest): # {{{
        # <nick> :<message>
        # This tells us our own nickname, and src tells us our remote server.
        if not src.is_server():
            raise ProtocolError('001 from non-server: %s' % src)
        (nick, rest) = rest.split(' ', 1)
        self._server = src
        u = User(nick, None, None)
        print 'Learned ourselves: %s' % u
        self._add_user(u)
        self._self = u
        self._add_event(Event('connected', src, u))
    # }}}
    def _parse_005(self, src, cmd, rest): # {{{
        # <nick> <005token>... :<message>
        # There are a lot of 005tokens.
        if not src.is_server():
            raise ProtocolError('005 from non-server: %s' % src)
        (nick, rest) = rest.split(' ', 1)
        while not rest.startswith(':'):
            (token, rest) = rest
            (key, val) = token.split('=', 1)
            key = key.lower()
            if not val and key in self._opts:
                self._opts[key] = True
            elif key == 'chantypes':
                self._opts['chantypes'] = val
            elif key == 'chanmodes':
                # ban-like, arg-for-both, arg-for-set, no-arg
                cmodes = val.split(',')
                self._opts['chanmodes'] = [cmodes[0], cmodes[1],
                                           cmodes[2], cmodes[3]]
            elif key == 'chanlimit':
                limits = val.split(',')
                res = {}
                for l in limits:
                    (key, val) = l.split(':', 1)
                    res[key] = val
                self._opts['chanlimit'] = res
            elif key == 'prefix':
                # Ick. (modes)sigils.
                (modes, sigils) = val.split(')', 1)
                modes = modes.strip('(')
                if len(modes) != len(sigils):
                    raise ProtocolError('len(modes) != len(sigils) in 005 PREFIX: %s' % val)
                for i in range(len(modes)):
                    self._opts['prefixes'][sigils[i]] = modes[i]
            elif key == 'maxlist':
                maxes = val.split(',')
                for m in maxes:
                    (lists, mv) = m.split(':', 1)
                    self._opts['maxlist'][lists] = mv
            elif key == 'modes':
                self._opts['modes'] = int(val)
            elif key == 'network':
                self._opts['network'] = val
            elif key == 'callerid':
                self._opts['callerid'] = val
            elif key == 'nicklen':
                self._opts['nicklen'] = int(val)
            elif key == 'channellen':
                self._opts['channellen'] = int(val)
            elif key == 'topiclen':
                self._opts['topiclen'] = int(val)
            elif key == 'deaf':
                self._opts['deaf'] = val
            elif key == 'monitor':
                self._opts['monitor'] = int(val)
            elif key == 'targmax':
                for targmax in val.split(','):
                    (k, v) = targmax.split(':', 1)
                    if not v:
                        next
                    k = k.lower()
                    self._opts['targmax'][k] = int(v)
            elif key == 'extban':
                (char, types) = val.split(',')
                self._opts['extban_char'] = char
                self._opts['extban_types'] = types
            elif key == 'clientver':
                self._opts['clientver'] = float(val)
            elif key == 'elist':
                self._opts['elist'] = val
    # }}}
    def _parse_join(self, src, cmd, rest): # {{{
        # <channel> [:message]
        if not src.is_user():
            raise ProtocolError('join from non-user source %s' % src)
        if src.nick() not in self._users:
            # We learned a new user!
            self._users[src.nick().lower()] = src
        if ' :' in rest:
            (chan, msg) = rest.split(' ', 1)
        else:
            chan = rest
        c = self._parsedest(chan)
        if not c.is_channel():
            raise ProtocolError('join to non-channel dest %s' % chan)
        if src == self._self and chan.lower() not in self._channels:
            # If we're joining, and we don't already know this channel, learn
            # it.
            self._channels[chan.lower()] = c
        self._add_event(Event('join', src, c))
        c._add_user(src)
    # }}}
    def _parse_chmode(self, src, dest, modes): # {{{
        pass
    # }}}
    def _parse_mode(self, src, cmd, rest): # {{{
        # <dest> <modes...>
        (dest, modes) = rest.split(' ', 1)
        if not dest:
            print '!?!?'
            return
        if dest.is_channel():
            self._parse_chmode(src, dest, modes)
    # }}}
    def _parse_privmsg(self, src, cmd, rest): # {{{
        # <target> :<text...>
        (dest, msg) = rest.split(' ', 1)
        dest = self._parsedest(dest)
        if msg[0] == ':':
            msg = msg[1:]
        self._add_event(Event('privmsg', src, dest, None, msg))
    # }}}
    def _parse_unknown(self, src, cmd, rest): # {{{
        print 'Ignored: %s %s "%s"' % (src, cmd, rest)
    # }}}
    def _parse(self, line): # {{{
        """
        Parses a single line of IRC input text, updating state and filling
        output and event buffers as appropriate.
        """
        line = line.replace('\n', '').replace('\r', '')

        # Oddball case with no source at all first.
        if line.lower().startswith('PING '):
            self._parsers['ping'](line)
            return

        # Now that we know we have a source, pull it out of the string.
        (src, rest) = line.split(' ', 1)
        src = self._parsesource(src)

        # Next up, the command.
        (cmd, rest) = rest.split(' ', 1)
        cmd = cmd.lower()

        # Now hand off to the command-specific parser.
        if cmd in self._parsers:
            self._parsers[cmd](src, cmd, rest)
        else:
            self._parsers['?'](src, cmd, rest)
    # }}}

    # Methods you might need to override to subclass this start here.
    def _add_output(self, line): # {{{
        """
        Adds a new line of output to the output buffer. You probably should not
        call this; use the quote() function instead. If you are subclassing IRC
        to provide notifications when new output is ready, override this.
        """
        self._output.append(line)
    # }}}
    def _add_event(self, event): # {{{
        """
        Adds a new event (an object with the interface of the Event class) to
        the event buffer. You probably should not call this directly. If you are
        subclassing IRC to provide notifications when new events are ready,
        override this.
        """
        self._events.append(event)
    # }}}

    # Public interface starts here
    def __init__(self, nick=None, user='ircpy', gecos='irc.py', password=None): # {{{
        """
        Initialize an instance. If nick is not given, a random nick is
        generated; if user is not given, user = nick.
        """
        self._parsers = {
            'ping': self._parse_ping,
            '001': self._parse_001,
            '005': self._parse_005,
            'join': self._parse_join,
            'mode': self._parse_mode,
            'privmsg': self._parse_privmsg,
            '?': self._parse_unknown,
        }
        self._opts = {
            # Conservative guesses for all option values if we don't get them in
            # 005.
            'chantypes': '#',
            'excepts': False,
            'invex': False,
            'chanmodes': [ 'b', 'k', 'l', 'imnpst' ],
            'prefixes': { '@': 'o', '+': 'v' },
            'chanlimit': { '#': 30 },
            'maxlist': { 'b': 30 },
            'modes': 1,
            'knock': False,
            'network': None,
            'nicklen': 8,
            'channellen': 16,
            'topiclen': 300,
            'etrace': False,
            'cprivmsg': False,
            'cnotice': False,
            'deaf': None,
            'monitor': None,
            'fnc': False,
            'targmax': {
                'names': 1,
                'list': 1,
                'kick': 1,
                'whois': 1,
                'privmsg': 1,
                'notice': 1,
                'accept': 1,
                'monitor': 1
            },
            'extban_char': None,
            'extban_types': None,
            'whox': False,
            'clientver': 1.0,
            'elist': None
        }
        self._random = random.Random()
        self._output = []
        self._events = []
        self._users = {}
        self._channels = {}
        self._servers = {}
        self._self = None
        self._server = None
        nick = nick or self._genstr('ircpy-XXXXXXXX')

        if password:
            self.add_output('PASS :%s' % password)
        self._add_output('NICK %s' % nick)
        self._add_output('USER %s %s "" :%s' % (user, user, gecos))
    # }}}
    def input(self, line): # {{{
        """
        Update this IRC session's state from a new line of input. May cause new
        lines of output to be generated, or new events to be generated.
        """
        self._parse(line)
    # }}}
    def output(self): # {{{
        """
        Returns all pending output for this IRC session. The returned output is
        removed from the output buffer.
        """
        out = self._output
        self._output = []
        return out
    # }}}
    def events(self): # {{{
        """
        Returns all pending events for this IRC session. The returned events are
        removed from the event buffer.
        """
        evts = self._events
        self._events = []
        return evts
    # }}}
    def user(self, nick=None): # {{{
        """
        Returns an IRC user by nickname, or None if no user by that nickname is
        known. If no nickname is supplied, returns the user of this session
        (i.e., the user as which we are connected).
        """
        if not nick:
            return self._self
        nick = nick.lower()
        if nick not in self._users:
            return None
        return self._users[nick]
    # }}}
    def users(self): # {{{
        """
        Returns all known users, as a nick -> user object dictionary.
        """
        return self._users
    # }}}
    def server(self, name=None): # {{{
        """
        Returns an IRC server by name, or None if no server by that name is
        known. If no name is supplied, returns the server this session is
        directly connected to.
        """
        if not name:
            return self._server
        name = name.lower()
        if name not in self._servers:
            return None
        return self._servers[name]
    # }}}
    def servers(self): # {{{
        """
        Returns all known servers, as a name -> server object dictionary.
        """
        return self._servers
    # }}}
    def channel(self, name): # {{{
        """
        Returns an IRC channel by name, or None if no channel by that name is
        known.
        """
        name = name.lower()
        if name not in self._channels:
            return None
        return self._channels[name]
    # }}}
    def channels(self): # {{{
        """
        Returns all known channels, as a name -> channel object dictionary.
        """
        return self._channels
    # }}}

    # IRC protocol methods
    def join(self, channel, key=None): # {{{
        if key:
            self._add_output('JOIN %s %s' % (channel, key))
        else:
            self._add_output('JOIN %s' % channel)
    # }}}
    def part(self, channel): # {{{
        self._add_output('PART %s' % channel)
    # }}}
    def msg(self, target, text): # {{{
        self._add_output('PRIVMSG %s :%s' % (target, text))

    # }}}

if __name__ == '__main__':
    irc = IRC(nick='tester')
    while True:
        outs = irc.output()
        evts = irc.events()
        for out in outs:
            print 'out: %s' % out
        for evt in evts:
            print 'evt: %s' % evt
        line = sys.stdin.readline()
        if not line:
            break
        irc.input(line)
