// Copyright 2011-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

#include <stdarg.h>
#include <string.h>

#include "link_local.h"
#include "sos/dev/bootloader.h"

static int reset_device(link_transport_mdriver_t *driver, int invoke_bootloader);

int link_bootloader_attr(
  link_transport_mdriver_t *driver,
  bootloader_attr_t *attr,
  u32 id) {
  link_errno = 0;
  int ret = link_ioctl(driver, LINK_BOOTLOADER_FILDES, I_BOOTLOADER_GETINFO, attr);
  if (ret < 0) {
    if (link_errno == 9) { // EBADF
      link_debug(LINK_DEBUG_MESSAGE, "Device is present but is not a bootloader");
      return LINK_DEVICE_PRESENT_BUT_NOT_BOOTLOADER;
    }
    return -1;
  }

  return 0;
}

int link_bootloader_attr_legacy(
  link_transport_mdriver_t *driver,
  bootloader_attr_t *attr,
  u32 id) {
  bootloader_attr_legacy_t legacy_attr;
  int ret;
  link_errno = 0;
  if (
    (ret = link_ioctl(
       driver, LINK_BOOTLOADER_FILDES, I_BOOTLOADER_GETATTR_LEGACY, &legacy_attr))
    < 0) {
    if (link_errno != 0) {
      link_debug(LINK_DEBUG_MESSAGE, "Legacy Device is present but is not a bootloader");
      return LINK_DEVICE_PRESENT_BUT_NOT_BOOTLOADER;
    }
    return -1;
  }

  memcpy(attr, &legacy_attr, sizeof(legacy_attr));
  attr->hardware_id =
    0x00000001; // CoAction Hero is the only board with legacy bootloader installed

  return 0;
}

int link_isbootloader_legacy(link_transport_mdriver_t *driver) {
  bootloader_attr_t attr;

  link_debug(LINK_DEBUG_INFO, "call with driver %p", driver->phy_driver.handle);

  int ret = link_bootloader_attr_legacy(driver, &attr, 0);
  if (ret == LINK_DEVICE_PRESENT_BUT_NOT_BOOTLOADER) {
    return 0;
  } else if (ret < 0) {
    return -1;
  }
  return 1;
}

int link_isbootloader(link_transport_mdriver_t *driver) {
  bootloader_attr_t attr;
  int ret;

  link_debug(LINK_DEBUG_INFO, "call with driver %p", driver->phy_driver.handle);

  ret = link_bootloader_attr(driver, &attr, 0);

  if (ret == LINK_DEVICE_PRESENT_BUT_NOT_BOOTLOADER) {
    return 0;
  } else if (ret < 0) {
    return -1;
  }

  // If the above succeeds, the bootloader is present
  return 1;
}

int link_reset(link_transport_mdriver_t *driver) {
  link_op_t op;
  link_debug(LINK_DEBUG_MESSAGE, "try to reset--check bootloader");
  if (link_isbootloader(driver)) {
    // execute the request
    op.ioctl.cmd = LINK_CMD_IOCTL;
    op.ioctl.fildes = LINK_BOOTLOADER_FILDES;
    op.ioctl.request = I_BOOTLOADER_RESET;
    op.ioctl.arg = 0;
    link_transport_mastersettimeout(driver, 10);
    link_transport_masterwrite(driver, &op, sizeof(link_ioctl_t));
    link_transport_mastersettimeout(driver, 0);
    driver->phy_driver.close(&(driver->phy_driver.handle));
  } else {
    link_debug(LINK_DEBUG_MESSAGE, "reset device with /dev/core");
    return reset_device(driver, 0);
  }
  return 0;
}

int reset_device(link_transport_mdriver_t *driver, int invoke_bootloader) {
  // use "/dev/core" to reset
  int fd;
  core_attr_t attr;
  int ret = 0;

  fd = link_open(driver, "/dev/core", LINK_O_RDWR);
  if (fd < 0) {
    ret = -1;
  } else {
    if (invoke_bootloader == 0) {
      link_debug(LINK_DEBUG_MESSAGE, "Try to reset");
      attr.o_flags = CORE_FLAG_EXEC_RESET;
    } else {
      link_debug(LINK_DEBUG_MESSAGE, "Try to invoke bootloader");
      attr.o_flags = CORE_FLAG_EXEC_INVOKE_BOOTLOADER;
    }

    link_transport_mastersettimeout(driver, 50);
    if (link_ioctl(driver, fd, I_CORE_SETATTR, &attr) < 0) {
      // try the old version of the bootloader
      if (link_ioctl(driver, fd, I_CORE_INVOKEBOOTLOADER_2, &attr) < 0) {
        ret = -1;
      }
    }
    link_transport_mastersettimeout(driver, 0);
    // since the device has been reset -- close the handle
    driver->transport_version = 0;
    driver->phy_driver.close(&(driver->phy_driver.handle));
  }
  return ret;
}

int link_resetbootloader(link_transport_mdriver_t *driver) {
  return reset_device(driver, 1);
}

int link_verify_signature(
  link_transport_mdriver_t *driver,
  const bootloader_attr_t *attr,
  const auth_signature_t *signature) {
  if (attr->version < 0x400) {
    return 0;
  }

  auth_signature_t tmp = *signature;
  return link_ioctl_delay(
    driver, LINK_BOOTLOADER_FILDES, I_BOOTLOADER_VERIFY_SIGNATURE, &tmp, 0, 0);
}

int link_get_public_key(
  link_transport_mdriver_t *driver,
  const bootloader_attr_t *attr,
  u8 *public_key,
  size_t public_key_size) {
  if (attr->version < 0x400) {
    printf("key is not supported\n");
    return 0;
  }

  if (public_key_size != 64) {
    printf("bad key size requested\n");
    return -1;
  }

  auth_public_key_t key = {};
  if (
    link_ioctl_delay(
      driver, LINK_BOOTLOADER_FILDES, I_BOOTLOADER_GET_PUBLIC_KEY, &key, 0, 0)
    < 0) {
    printf("failed to get the key\n");
    return -2;
  }

  memcpy(public_key, key.data, sizeof(key.data));
  return 0;
}

int link_is_signature_required(
  link_transport_mdriver_t *driver,
  const bootloader_attr_t *attr) {
  if (attr->version < 0x400) {
    return 0;
  }

  return link_ioctl_delay(
    driver, LINK_BOOTLOADER_FILDES, I_BOOTLOADER_IS_SIGNATURE_REQUIRED, NULL, 0, 0);
}

int link_eraseflash(link_transport_mdriver_t *driver) {
  if (
    link_ioctl_delay(driver, LINK_BOOTLOADER_FILDES, I_BOOTLOADER_ERASE, NULL, 0, 0)
    < 0) {
    return -1;
  }
  return 0;
}

int link_readflash(link_transport_mdriver_t *driver, int addr, void *buf, int nbyte) {
  link_op_t op;
  link_reply_t reply;
  int err;
  int len;

  op.read.cmd = LINK_CMD_READ;
  op.read.addr = addr;
  op.read.nbyte = nbyte;

  link_debug(LINK_DEBUG_MESSAGE, "write read flash op");
  err = link_transport_masterwrite(driver, &op, sizeof(link_read_t));
  if (err < 0) {
    return err;
  }

  link_debug(LINK_DEBUG_MESSAGE, "read flash data");
  len = link_transport_masterread(driver, buf, nbyte);
  if (len < 0) {
    return LINK_TRANSFER_ERR;
  }

  link_debug(LINK_DEBUG_MESSAGE, "read reply");
  err = link_transport_masterread(driver, &reply, sizeof(reply));
  if (err < 0) {
    return err;
  }

  if (reply.err < 0) {
    link_errno = reply.err_number;
  }

  link_debug(LINK_DEBUG_MESSAGE, "Read %d bytes from device", reply.err);

  return reply.err;
}

int link_writeflash(
  link_transport_mdriver_t *driver,
  int addr,
  const void *buf,
  int nbyte) {
  bootloader_writepage_t wattr;
  int page_size;
  int bytes_written;
  int err;

  bytes_written = 0;
  wattr.addr = addr;
  page_size = BOOTLOADER_WRITEPAGESIZE;
  if (page_size > nbyte) {
    page_size = nbyte;
  }
  wattr.nbyte = page_size;

  link_debug(LINK_DEBUG_MESSAGE, "Page size is %d (%d)", page_size, nbyte);

  do {
    memset(wattr.buf, 0xFF, BOOTLOADER_WRITEPAGESIZE);
    memcpy(wattr.buf, buf, page_size);

    link_transport_mastersettimeout(driver, 5000);
    err = link_ioctl_delay(
      driver, LINK_BOOTLOADER_FILDES, I_BOOTLOADER_WRITEPAGE, &wattr, 0, 0);
    link_transport_mastersettimeout(driver, 0);
    if (err < 0) {
      link_error("I_BOOTLOADER_WRITEPAGE failed");
      return err;
    }

    wattr.addr += page_size;
    buf += page_size;
    bytes_written += page_size;

  } while (bytes_written < nbyte);
  return nbyte;
}
