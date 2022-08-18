// Copyright (c) 2022 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include <common/src/common.h>
#include "linux_genlfamilies.h"
#include "linux_libnl.h"

namespace
{
    enum : int
    {
        NlctrlVersion = 2
    };

    // Attribute validation definitions for nlctrl
    class NlctrlAttrs
    {
    public:
        NlctrlAttrs() : _familyAttrs{}, _mcastGrpAttrs{}
        {
            for(auto &policy : _familyAttrs)
                policy.type = NLA_UNSPEC;
            for(auto &policy : _mcastGrpAttrs)
                policy.type = NLA_UNSPEC;

            _familyAttrs[CTRL_ATTR_FAMILY_ID].type = NLA_U16;
            _familyAttrs[CTRL_ATTR_FAMILY_NAME].type = NLA_STRING;
            _familyAttrs[CTRL_ATTR_FAMILY_NAME].maxlen = GENL_NAMSIZ;
            _familyAttrs[CTRL_ATTR_VERSION].type = NLA_U32;
            _familyAttrs[CTRL_ATTR_MCAST_GROUPS].type = NLA_NESTED;

            _mcastGrpAttrs[CTRL_ATTR_MCAST_GRP_NAME].type = NLA_STRING;
            _mcastGrpAttrs[CTRL_ATTR_MCAST_GRP_ID].type = NLA_U32;
        }

    public:
        // Note - these return non-const references because libnl wants
        // non-const pointers to the nla_policy array; don't modify the data
        // through these
        constexpr auto &familyAttrs() {return _familyAttrs;}
        constexpr auto &mcastGrpAttrs() {return _mcastGrpAttrs;}

    private:
        std::array<nla_policy, CTRL_ATTR_MAX+1> _familyAttrs;
        std::array<nla_policy, CTRL_ATTR_MCAST_GRP_MAX+1> _mcastGrpAttrs;
    };

    NlctrlAttrs nlctrlAttrs;

    // Iterable view of an attribute containing nested objects.  Iterates
    // nlattr values.
    class NestedNlattrView
    {
    public:
        class Iterator
        {
        public:
            Iterator() : _pInnerAttr{nullptr} {} // End iterator
            Iterator(libnl::nlattr &nestedAttr)
                : _pInnerAttr{reinterpret_cast<libnl::nlattr*>(libnl::nla_data(&nestedAttr))},
                  _remaining{libnl::nla_len(&nestedAttr)}
            {
                checkEnd();
            }

        private:
            // After getting a new _pInnerAttr (either initially or from
            // ::nla_next()), make sure it's readable with ::nla_ok() - if not,
            // nulls out _pInnerAttr.
            void checkEnd()
            {
                // If there isn't room for a single attribute, we're already
                // done, become an end iterator
                if(_pInnerAttr && !libnl::nla_ok(_pInnerAttr, _remaining))
                    _pInnerAttr = nullptr;
            }

        public:
            bool operator==(const Iterator &other) const {return _pInnerAttr == other._pInnerAttr;}
            bool operator!=(const Iterator &other) const {return !(*this == other);}

            Iterator &operator++()
            {
                if(_pInnerAttr)
                {
                    _pInnerAttr = libnl::nla_next(_pInnerAttr, &_remaining);
                    checkEnd();
                }
                return *this;
            }
            libnl::nlattr &operator*() const {return *_pInnerAttr;}

        private:
            libnl::nlattr *_pInnerAttr;    // Current inner attribute; nullptr at end
            // Remaining data from outer attribute - needed for nla_next().
            // Does not affect comparisons.
            int _remaining;
        };

    public:
        NestedNlattrView(libnl::nlattr &nestedAttr) : _nestedAttr{nestedAttr} {}

        Iterator begin() {return {_nestedAttr};}
        Iterator end() {return {};}

    private:
        libnl::nlattr &_nestedAttr;
    };
}

LinuxGenlFamilies::LinuxGenlFamilies()
    : LinuxNlNtfSock{NETLINK_GENERIC}, _ready{false},
      _addedNotifyGroup{false}
{
    // Trigger a dump of generic families.  This will include the "notify"
    // multicast group for the nlctrl family, which we need to begin
    // notifications.
    NlUniquePtr<libnl::nl_msg> pDumpMsg{libnl::nlmsg_alloc()};
    libnl::genlmsg_put(pDumpMsg.get(), NL_AUTO_PORT, NL_AUTO_SEQ, GENL_ID_CTRL, 0,
                       NLM_F_DUMP, CTRL_CMD_GETFAMILY, NlctrlVersion);
    sendAuto(pDumpMsg.get());
}

QString LinuxGenlFamilies::readAttrString(libnl::nlattr &attr)
{
    int len = libnl::nla_len(&attr);
    // Check for empty string (so we can check the last character below for
    // non-empty strings)
    if(len <= 0)
        return {};

    const char *data = reinterpret_cast<const char*>(libnl::nla_data(&attr));

    // If there's a terminating null character, don't include it
    if(!data[len-1])
        --len;

    return QString::fromLatin1(data, len);
}

void LinuxGenlFamilies::handleNewFamily(libnl::nlmsghdr &msgHeader)
{
    std::array<libnl::nlattr*, nlctrlAttrs.familyAttrs().size()> attrs;
    auto parseErr = libnl::nlmsg_parse(&msgHeader, GENL_HDRLEN, attrs.data(),
                                       attrs.size()-1,
                                       nlctrlAttrs.familyAttrs().data());
    LibnlError::checkRet(parseErr, HERE, "Could not parse CTRL_CMD_NEWFAMILY message");

    // We need these fields.  CTRL_ATTR_MCAST_GROUPS is optional; it isn't
    // given if the family does not have any multicast groups.
    if(!attrs[CTRL_ATTR_FAMILY_ID] || !attrs[CTRL_ATTR_FAMILY_NAME] ||
       !attrs[CTRL_ATTR_VERSION])
    {
        throw LibnlError{HERE, "CTRL_CMD_NEWFAMILY message did not have all expected attributes"};
    }

    // Parse the multicast groups before touching the family in the map in case
    // this fails.
    std::unordered_map<QString, std::uint32_t> newMcastGroups;
    if(attrs[CTRL_ATTR_MCAST_GROUPS])
    {
        for(libnl::nlattr &mcastGroup : NestedNlattrView{*attrs[CTRL_ATTR_MCAST_GROUPS]})
        {
            std::array<libnl::nlattr*, nlctrlAttrs.mcastGrpAttrs().size()> grpAttrs;
            auto grpParseErr = libnl::nla_parse_nested(grpAttrs.data(), grpAttrs.size()-1,
                                                       &mcastGroup,
                                                       nlctrlAttrs.mcastGrpAttrs().data());
            LibnlError::checkRet(grpParseErr, HERE, "Could not parse CTRL_CMD_NEWFAMILY multicast groups");
            if(!grpAttrs[CTRL_ATTR_MCAST_GRP_ID] || !grpAttrs[CTRL_ATTR_MCAST_GRP_NAME])
                throw LibnlError{HERE, "CTRL_CMD_NEWFAMILY multicast group did not have all expected attributes"};

            QString grpName = readAttrString(*grpAttrs[CTRL_ATTR_MCAST_GRP_NAME]);
            newMcastGroups[grpName] = libnl::nla_get_u32(grpAttrs[CTRL_ATTR_MCAST_GRP_ID]);
        }
    }

    QString name = readAttrString(*attrs[CTRL_ATTR_FAMILY_NAME]);

    Family &family = _families[name];
    family._protocol = libnl::nla_get_u16(attrs[CTRL_ATTR_FAMILY_ID]);
    family._version = libnl::nla_get_u32(attrs[CTRL_ATTR_VERSION]);
    family._multicastGroups = std::move(newMcastGroups);

    qDebug() << "New generic netlink family" << name << "(id" << family._protocol
        << "- version" << family._version << "-"
        << family._multicastGroups.size() << "mcast groups)";
}

void LinuxGenlFamilies::handleDelFamily(libnl::nlmsghdr &msgHeader)
{
    std::array<libnl::nlattr*, nlctrlAttrs.familyAttrs().size()> attrs;
    auto parseErr = libnl::nlmsg_parse(&msgHeader, GENL_HDRLEN, attrs.data(),
                                       attrs.size()-1,
                                       nlctrlAttrs.familyAttrs().data());
    LibnlError::checkRet(parseErr, HERE, "Could not parse CTRL_CMD_DELFAMILY message");

    if(!attrs[CTRL_ATTR_FAMILY_NAME])
        throw LibnlError{HERE, "CTRL_CMD_DELFAMILY message did not have all expected attributes"};

    QString name = readAttrString(*attrs[CTRL_ATTR_FAMILY_NAME]);
    _families.erase(name);

    qDebug() << "Removed family" << name;
}

int LinuxGenlFamilies::handleMsg(nl_msg *pMsg)
{
    Q_ASSERT(pMsg); // Guarantee by libnl

    libnl::nlmsghdr *pHeader = libnl::nlmsg_hdr(pMsg);
    libnl::genlmsghdr *pGenHeader = libnl::genlmsg_hdr(pHeader);

    // Sanity check - should be an nlctrl message and version 2
    LibnlError::verify(pHeader->nlmsg_type == GENL_ID_CTRL, HERE, "Unexpected netlink message type");
    LibnlError::verify(pGenHeader->version == NlctrlVersion, HERE, "Unsupported nlctrl version");

    // Check for supported commands
    switch(pGenHeader->cmd)
    {
        case CTRL_CMD_NEWFAMILY:
        {
            handleNewFamily(*pHeader);

            // If we haven't added the notify group yet, check if the nlctrl
            // family was just added
            const Family *pNewNlctrlFamily = nullptr;
            if(!_addedNotifyGroup)
                pNewNlctrlFamily = getFamily(QStringLiteral("nlctrl"));

            if(pNewNlctrlFamily)
            {
                auto itNotifyGrp = pNewNlctrlFamily->_multicastGroups.find(QStringLiteral("notify"));
                if(itNotifyGrp != pNewNlctrlFamily->_multicastGroups.end())
                {
                    qDebug() << "nlctrl notify group:" << itNotifyGrp->second;
                    addMembership(itNotifyGrp->second);
                    _addedNotifyGroup = true;
                }
                else
                {
                    qWarning() << "nlctrl family was added without notify group";
                }
            }

            break;
        }
        case CTRL_CMD_DELFAMILY:
            handleDelFamily(*pHeader);
            break;
        // Commands for operations and multicast groups aren't supported yet.
        // We don't currently use any netlink families that have dynamic
        // operations or multicast groups.
        default:
            break;
    }

    return 0;
}

int LinuxGenlFamilies::handleFinish(libnl::nl_msg *)
{
    // This indicates that the initial dump has completed, all we do is indicate
    // that we're 'ready' - if nl80211 exists, we'd know about it by now
    if(!_ready)
    {
        qInfo() << "Initial genl families dump is ready";
        _ready = true;
    }

    return NL_OK;
}

auto LinuxGenlFamilies::getFamily(const QString &name) -> const Family *
{
    auto itFamily = _families.find(name);
    if(itFamily != _families.end())
        return &(itFamily->second);
    return nullptr;
}
