#include "pf/base/log.h"
#include "pf/base/time_manager.h"
#include "pf/net/packet/factorymanager.h"
#include "pf/net/connection/base.h"

namespace pf_net {

namespace connection {

Base::Base() {
  __ENTER_FUNCTION
    id_ = ID_INVALID;
    userid_ = ID_INVALID;
    managerid_ = ID_INVALID;
    socket_ = NULL;
    socket_inputstream_ = NULL;
    socket_outputstream_ = NULL;
    isempty_ = true;
    isdisconnect_ = false;
    isinit_ = false;
    packetindex_ = 0;
    execute_count_pretick_ = NET_CONNECTION_EXECUTE_COUNT_PRE_TICK_DEFAULT;
    receive_bytes_ = 0;
    send_bytes_ = 0;
    status_ = 0;
  __LEAVE_FUNCTION
}

Base::~Base() {
  __ENTER_FUNCTION
    SAFE_DELETE(socket_outputstream_);
    SAFE_DELETE(socket_inputstream_);
    SAFE_DELETE(socket_);
  __LEAVE_FUNCTION
}

bool Base::init() {
  __ENTER_FUNCTION
    if (isinit()) return true; //防止再次初始化，会出错
    socket_ = new socket::Base();
    Assert(socket_);
    socket_inputstream_ = new socket::InputStream(
        socket_,
        SOCKETINPUT_BUFFERSIZE_DEFAULT,
        64 * 1024 * 1024
        );
    Assert(socket_inputstream_);
    socket_outputstream_ = new socket::OutputStream(
        socket_,
        SOCKETOUTPUT_BUFFERSIZE_DEFAULT,
        64 * 1024 * 1024);
    Assert(socket_outputstream_);
    isinit_ = true;
    return true;
  __LEAVE_FUNCTION
    return false;
}

bool Base::processinput() {
  __ENTER_FUNCTION
    bool result = false;
    if (isdisconnect()) return true;
    try {
      int32_t fillresult = socket_inputstream_->fill();
      
      if (fillresult <= SOCKET_ERROR) {
        char errormessage[FILENAME_MAX];
        memset(errormessage, '\0', sizeof(errormessage));
        socket_inputstream_->getsocket()->getlast_errormessage(
            errormessage, 
            static_cast<uint16_t>(sizeof(errormessage) - 1));
        SLOW_ERRORLOG(NET_MODULENAME,
                      "[net.connection] (connection::Base::processinput)"
                      " socket_inputstream_->fill() result: %d %s",
                      fillresult,
                      errormessage);
        result = false;
      } else {
        result = true;
        receive_bytes_ += static_cast<uint32_t>(fillresult); //网络流量
      }
    } catch(...) {
      SaveErrorLog();
    }
    return result;
  __LEAVE_FUNCTION
    return false;
}

bool Base::processoutput() {
  __ENTER_FUNCTION
    bool result = false;
    if (isdisconnect()) return true;
    try {
      uint32_t size = socket_outputstream_->reallength();
      if (0 == size) return true;
      int32_t flushresult = socket_outputstream_->flush();
      if (flushresult <= SOCKET_ERROR) {
        char errormessage[FILENAME_MAX] = {0};
        socket_inputstream_->getsocket()->getlast_errormessage(
            errormessage, 
            static_cast<uint16_t>(sizeof(errormessage) - 1));
        SLOW_ERRORLOG(NET_MODULENAME,
                      "[net.connection] (Base::processoutput)"
                      " socket_outputstream_->flush() result: %d %s",
                      flushresult,
                      errormessage);
        result = false;
      } else {
        result = true;
        send_bytes_ += static_cast<uint32_t>(flushresult);
      }
    } catch(...) {
      SaveErrorLog();
    }
    return result;
  __LEAVE_FUNCTION
    return false;
}

bool Base::processcommand(bool option) {
  __ENTER_FUNCTION
    bool result = false;
    char packetheader[PACKET_HEADERSIZE] = {'\0'};
    uint16_t packetid;
    uint32_t packetcheck, packetsize, packetindex;
    packet::Base* packet = NULL;
    if (isdisconnect()) return true;
    try {
      if (option) { //执行选项操作
      }
      uint32_t i;
      for (i = 0; i < execute_count_pretick_; ++i) {
        if (!socket_inputstream_->peek(&packetheader[0], PACKET_HEADERSIZE)) {
          //数据不能填充消息头
          break;
        }
        memcpy(&packetid, &packetheader[0], sizeof(uint16_t));
        memcpy(&packetcheck, &packetheader[sizeof(uint16_t)], sizeof(uint32_t));
        packetsize = GET_PACKETLENGTH(packetcheck);
        packetindex = GET_PACKETINDEX(packetcheck);
        if (!g_packetfactory_manager->isvalid_packetid(packetid)) {
          return false;
        }
        try {
          //check packet length
          if (socket_inputstream_->reallength() < 
              PACKET_HEADERSIZE + packetsize) {
            //message not receive full
            break;
          }
          //check packet size
          if (packetsize > 
              g_packetfactory_manager->getpacket_maxsize(packetid)) {
            char temp[FILENAME_MAX] = {0};
            snprintf(temp, 
                     sizeof(temp) - 1, 
                     "packet size error, packetid = %d", 
                     packetid);
            AssertEx(false, temp);
            return false;
          }
          //create packet
          packet = g_packetfactory_manager->createpacket(packetid);
          if (NULL == packet) return false;
          packet->setindex(static_cast<int8_t>(packetindex));
          
          //read packet
          result = socket_inputstream_->readpacket(packet);
          if (false == result) {
            g_packetfactory_manager->removepacket(packet);
            return result;
          }
          bool needremove = true;
          bool exception = false;
          uint32_t executestatus = 0;
          try {
            resetkick();
            try {
              executestatus = packet->execute(this);
            } catch(...) {
              SaveErrorLog();
              executestatus = kPacketExecuteStatusError;
            }
            if (kPacketExecuteStatusError == executestatus) {
              if (packet) g_packetfactory_manager->removepacket(packet);
              return false;
            } else if (kPacketExecuteStatusBreak == executestatus) {
              if (packet) g_packetfactory_manager->removepacket(packet);
              break;
            } else if (kPacketExecuteStatusContinue == executestatus) {
              //continue read last packet
            } else if (kPacketExecuteStatusNotRemove == executestatus) {
              needremove = false;
            } else if (kPacketExecuteStatusNotRemoveError == executestatus) {
              return false;
            } else {
              //unknown status
            }
          } catch(...) {
            SaveErrorLog();
            exception = true;
          }
          if (packet && needremove) 
            g_packetfactory_manager->removepacket(packet);
          if (exception) return false;
        } catch(...) {
          SaveErrorLog();
          return false;
        }
      }
    } catch(...) {
      SaveErrorLog();
      return false;
    }
    return true;
  __LEAVE_FUNCTION
    return false;
}

bool Base::sendpacket(packet::Base* packet) {
  __ENTER_FUNCTION
    bool result = false;
    if (isdisconnect()) return true;
    if (socket_outputstream_ != NULL) {
      packet->setindex(++packetindex_);
      uint32_t before_writesize = socket_outputstream_->reallength();
      result = socket_outputstream_->writepacket(packet);
      Assert(result);
      uint32_t after_writesize = socket_outputstream_->reallength();
      if (packet->getsize() != after_writesize - before_writesize - 6) {
        FAST_ERRORLOG(kApplicationLogFile,
                      "[net.connection] (Base::sendpacket) size error"
                      "id = %d(write: %d, should: %d)",
                      packet->getid(),
                      after_writesize - before_writesize - 6,
                      packet->getsize());
        result = false;
      }
      /**
      if (kPacketIdSCCharacterIdle == packet->getid()) {
        //save heartbeat log
      }
      **/
    }
    return result;
  __LEAVE_FUNCTION
    return false;
}

bool Base::heartbeat(uint32_t time, uint32_t flag) {
  USE_PARAM(time);
  USE_PARAM(flag);
  return true;
}

int16_t Base::getid() const {
  return id_;
}

void Base::setid(int16_t id) {
  id_ = id;
}

int16_t Base::get_userid() const {
  return userid_;
}

void Base::set_userid(int16_t id) {
  userid_ = id;
}

int16_t Base::get_managerid() const {
  return managerid_;
}

void Base::set_managerid(int16_t id) {
  managerid_ = id;
}

socket::Base* Base::getsocket() {
  return socket_;
}

uint8_t Base::get_execute_count_pretick() const {
  return execute_count_pretick_;
}

void Base::set_execute_count_pretick(uint8_t count) {
  execute_count_pretick_ = count;
}

void Base::disconnect() {
  __ENTER_FUNCTION
    socket_->close();
  __LEAVE_FUNCTION
}

bool Base::isvalid() {
  __ENTER_FUNCTION
    if (NULL == socket_) return false;
    bool result = false;
    result = socket_->isvalid();
    return result;
  __LEAVE_FUNCTION
    return false;
}

void Base::cleanup() {
  __ENTER_FUNCTION
    if (socket_) socket_->close();
    if (socket_inputstream_) socket_inputstream_->cleanup();
    if (socket_outputstream_) socket_outputstream_->cleanup();
    set_managerid(ID_INVALID);
    set_userid(ID_INVALID);
    packetindex_ = 0;
    status_ = 0;
    execute_count_pretick_ = NET_CONNECTION_EXECUTE_COUNT_PRE_TICK_DEFAULT;
    setdisconnect(false);
    setempty(true);
  __LEAVE_FUNCTION
}

bool Base::isempty() const {
  return isempty_;
}

void Base::setempty(bool status) {
  isempty_ = status;
}

bool Base::isdisconnect() const {
  return isdisconnect_;
}

void Base::setdisconnect(bool status) {
  isdisconnect_ = status;
}

void Base::resetkick() {
  //do nothing
}

uint32_t Base::get_receive_bytes() {
  __ENTER_FUNCTION
    uint32_t result = receive_bytes_;
    receive_bytes_ = 0;
    return result;
  __LEAVE_FUNCTION
    return 0;
}

uint32_t Base::get_send_bytes() {
  __ENTER_FUNCTION
    uint32_t result = send_bytes_;
    send_bytes_ = 0;
    return result;
  __LEAVE_FUNCTION
    return 0;
}

bool Base::isinit() const {
  return isinit_;
}

void Base::setstatus(uint32_t status) {
  status_ = status;
}

uint32_t Base::getstatus() const {
  return status_;
}

bool Base::isserver() const {
  return true;
}

bool Base::isplayer() const {
  return false;
}

} //namespace connection

} //namespace pf_net
