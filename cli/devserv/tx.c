/*

  Copyright (C) 2021 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#define SU_LOG_DOMAIN "analyzer-server-tx"

#include "devserv.h"
#include <util/compat-poll.h>
#include <util/compat-socket.h>
#include <analyzer/msg.h>
#include <sys/fcntl.h>
#include <zlib.h>

#ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
#endif

SUPRIVATE void
suscli_analyzer_client_tx_thread_dispose_buffer(
    struct suscli_analyzer_client_tx_thread *self,
    grow_buf_t *buffer)
{
  grow_buf_clear(buffer);

  if (!suscan_mq_write(&self->pool, 0, buffer)) {
    grow_buf_finalize(buffer);
    free(buffer);
  }
}

SUPRIVATE grow_buf_t *
suscli_analyzer_client_tx_thread_alloc_buffer(
    struct suscli_analyzer_client_tx_thread *self)
{
  grow_buf_t *new = NULL;

  if (suscan_mq_poll(&self->pool, NULL, (void **) &new))
    return new;

  /* No recently released buffers. Time to allocate a new one. */

  SU_TRYCATCH(new = calloc(1, sizeof(grow_buf_t)), return NULL);

  return new;
}

SUINLINE SUBOOL
suscli_analyzer_client_tx_thread_helper_send(
  int fd,
  const void *data,
  size_t size)
{
  const uint8_t *bytes = data;
  size_t p = 0;
  ssize_t got = 0;

  while (p < size) {
    got = send(fd, bytes + p, size - p, MSG_NOSIGNAL);
    if (got == 0) {
      SU_ERROR("send(): connection closed by foreign host\n");
      return SU_FALSE;
    } else if (got < 0) {
      SU_ERROR("send(): error: %s\n", strerror(errno));
      return SU_FALSE;
    }

    p += got;
  }

  return SU_TRUE;
}

SUINLINE SUBOOL
suscli_analyzer_client_tx_thread_write_buffer_internal(
    struct suscli_analyzer_client_tx_thread *self,
    uint32_t magic,
    const grow_buf_t *buffer)
{
  struct suscan_analyzer_remote_pdu_header header;
  const uint8_t *data;
  size_t size, chunksize;
  SUBOOL ok = SU_FALSE;

  data = grow_buf_get_buffer(buffer);
  size = grow_buf_get_size(buffer);

  header.magic = htonl(magic);
  header.size  = htonl(size);

  chunksize = sizeof(struct suscan_analyzer_remote_pdu_header);

  if (!suscli_analyzer_client_tx_thread_helper_send(
    self->fd,
    &header,
    chunksize))
    goto done;

  /*
   * Calls can be extremely big, so we better send them in small chunks. Also,
   * this loop may be slow and thread may have been cancelled in the
   * meantime. Check for every chunk whether it was cancelled before
   * continuing.
   */

  while (size > 0 && !self->thread_cancelled) {
    chunksize = size;
    if (chunksize > SUSCAN_REMOTE_READ_BUFFER)
      chunksize = SUSCAN_REMOTE_READ_BUFFER;

    if (!suscli_analyzer_client_tx_thread_helper_send(
      self->fd,
      data,
      chunksize))
      goto done;

    data += chunksize;
    size -= chunksize;
  }

  ok = SU_TRUE;

done:
  return ok;
}

SUINLINE SUBOOL
suscli_analyzer_client_tx_thread_write_compressed_buffer(
    struct suscli_analyzer_client_tx_thread *self,
    const grow_buf_t *buffer)
{
  grow_buf_t compressed = grow_buf_INITIALIZER;
  SUBOOL ok = SU_FALSE;
  
  SU_TRYCATCH(
    suscan_remote_deflate_pdu((grow_buf_t *) buffer, &compressed),
    goto done);

  SU_TRYCATCH(
    suscli_analyzer_client_tx_thread_write_buffer_internal(
      self, 
      SUSCAN_REMOTE_COMPRESSED_PDU_HEADER_MAGIC, 
      &compressed),
    goto done);

  ok = SU_TRUE;

done:
  grow_buf_finalize(&compressed);

  return ok;
}

SUPRIVATE SUBOOL
suscli_analyzer_client_tx_thread_write_buffer(
    struct suscli_analyzer_client_tx_thread *self,
    const grow_buf_t *buffer)
{
  if (self->compress_threshold > 0 
    && grow_buf_get_size(buffer) > self->compress_threshold)
    return suscli_analyzer_client_tx_thread_write_compressed_buffer(
      self, 
      buffer);
  else
    return suscli_analyzer_client_tx_thread_write_buffer_internal(
      self,
      SUSCAN_REMOTE_PDU_HEADER_MAGIC,
      buffer);
}

SUPRIVATE void *
suscli_analyzer_client_tx_thread_func(void *userdata)
{
  struct suscli_analyzer_client_tx_thread *self =
      (struct suscli_analyzer_client_tx_thread *) userdata;
  struct pollfd pollfds[2];
  char b;
  uint32_t type;
  grow_buf_t *buffer = NULL;

  while ((buffer = suscan_mq_read(&self->queue, &type)) != NULL) {
    /* Cancelled via MQ. We should not reach this point in this impl. */
    if (type == SUSCLI_ANALYZER_CLIENT_TX_CANCEL)
      goto done;

    pollfds[0].events  = POLLOUT | POLLERR | POLLHUP;
    pollfds[0].fd      = self->fd;
    pollfds[0].revents = 0;

    pollfds[1].events  = POLLIN;
    pollfds[1].fd      = self->cancel_pipefd[0];
    pollfds[1].revents = 0;

    SU_TRYCATCH(poll(pollfds, 2, -1) != -1, goto done);

    /* Cancelled via cancelfd */
    if (pollfds[1].revents & POLLIN) {
      IGNORE_RESULT(int, read(self->cancel_pipefd[0], &b, 1));
      goto done;
    }

    if (pollfds[0].revents != 0) {
      if (pollfds[0].revents & POLLOUT) {
        SU_TRYCATCH(
            suscli_analyzer_client_tx_thread_write_buffer(self, buffer),
            goto done);
      } else {
        /* Impossible to write to this fd, give up */
        goto done;
      }
    }

    suscli_analyzer_client_tx_thread_dispose_buffer(self, buffer);
    buffer = NULL;
  }

done:
  if (buffer != NULL) {
    grow_buf_finalize(buffer);
    free(buffer);
  }

  self->thread_finished = SU_TRUE;

  return NULL;
}

SUPRIVATE void
suscli_analyzer_client_tx_thread_cancel(
    struct suscli_analyzer_client_tx_thread *self)
{
  char b = 1;

  self->thread_cancelled = SU_TRUE;

  /*
   * Order is important here. If write is called first but tx thread
   * is waiting on suscan_mq_read, this will block and the cancel message
   * may never be sent.
   */
  suscan_mq_write_urgent(&self->queue, SUSCLI_ANALYZER_CLIENT_TX_CANCEL, NULL);

  IGNORE_RESULT(int, write(self->cancel_pipefd[1], &b, 1));
}

SUPRIVATE void
suscli_analyzer_client_tx_thread_cancel_soft(
    struct suscli_analyzer_client_tx_thread *self)
{
  suscan_mq_write(&self->queue, SUSCLI_ANALYZER_CLIENT_TX_CANCEL, NULL);
}

SUPRIVATE void
suscli_analyzer_client_tx_consume_buffer_mq(struct suscan_mq *mq)
{
  grow_buf_t *buffer;

  while (suscan_mq_poll(mq, NULL, (void **) &buffer)) {
    /* Null messages are used to notify special conditions */
    if (buffer != NULL) {
      grow_buf_finalize(buffer);
      free(buffer);
    }
  }
}

void
suscli_analyzer_client_tx_thread_stop(
  struct suscli_analyzer_client_tx_thread *self)
{
   if (self->thread_running) {
    if (!self->thread_finished)
      suscli_analyzer_client_tx_thread_cancel(self);

    pthread_join(self->thread, NULL);

    self->thread_running = SU_FALSE;
  }
}

void
suscli_analyzer_client_tx_thread_stop_soft(
  struct suscli_analyzer_client_tx_thread *self)
{
  if (self->thread_running) {
    if (!self->thread_finished)
      suscli_analyzer_client_tx_thread_cancel_soft(self);

    pthread_join(self->thread, NULL);

    self->thread_running = SU_FALSE;
  }
}

void
suscli_analyzer_client_tx_thread_finalize(
    struct suscli_analyzer_client_tx_thread *self)
{
  suscli_analyzer_client_tx_thread_stop(self);

  if (self->pool_initialized)
    suscli_analyzer_client_tx_consume_buffer_mq(&self->pool);

  if (self->queue_initialized)
    suscli_analyzer_client_tx_consume_buffer_mq(&self->queue);

  if (self->cancel_pipefd[0] > 0 && self->cancel_pipefd[1] > 0) {
    close(self->cancel_pipefd[0]);
    close(self->cancel_pipefd[1]);
  }
}

SUBOOL
suscli_analyzer_client_tx_thread_push_zerocopy(
    struct suscli_analyzer_client_tx_thread *self,
    grow_buf_t *pdu)
{
  SUBOOL ok = SU_FALSE;
  grow_buf_t *buffer = NULL;

  SU_TRYCATCH(
      buffer = suscli_analyzer_client_tx_thread_alloc_buffer(self),
      goto done);

  grow_buf_transfer(buffer, pdu);

  SU_TRYCATCH(
      suscan_mq_write(&self->queue, SUSCLI_ANALYZER_CLIENT_TX_MESSAGE, buffer),
      goto done);
  buffer = NULL;

  ok = SU_TRUE;

done:
  if (buffer != NULL) {
    grow_buf_finalize(buffer);
    free(buffer);
  }

  return ok;
}

SUBOOL
suscli_analyzer_client_tx_thread_push(
    struct suscli_analyzer_client_tx_thread *self,
    const grow_buf_t *pdu)
{
  SUBOOL ok = SU_FALSE;
  void *buf;
  grow_buf_t copy = grow_buf_INITIALIZER;

  SU_TRYCATCH(
      buf = grow_buf_alloc(&copy, grow_buf_get_size(pdu)),
      goto done);

  memcpy(buf, grow_buf_get_buffer(pdu), grow_buf_get_size(pdu));

  SU_TRYCATCH(
      suscli_analyzer_client_tx_thread_push_zerocopy(self, &copy),
      goto done);

  ok = SU_TRUE;

done:
  grow_buf_finalize(&copy);

  return ok;
}

/* Cleanup callbacks */
struct suscli_analyzer_client_tx_thread_cleanup_ctx
{
  struct suscan_mq *mq;
  grow_buf_t       *head_source_info;
  SUBOOL            critical_reached;
  unsigned int      discarded;
};

/* 
 * This is what the cleanup strategy looks like: we classify messages
 * in three categories:
 *   - Overridable messages
 *   - Discardable messages
 *   - Critical messages (other)
 * 
 * Overridable messages are messages whose validity is revoked by
 * posterior messages. This is the case for source info messages
 * 
 * Discardable messages are messages that are delivered to the client
 * in a "best-effort" policy. It is desirable that they arrive to the
 * client, but can be discarded safely if the network does not support
 * such rates. This is the case for non-loop PSD messages.
 * 
 * Other messages are critical, and are needed to be delivered always,
 * in order, to keep the client in sync with the server.
 * 
 * --------8<---------------------------------------------------------
 * 
 * In case we need to perform an emergency cleanup, we discard all PSD
 * messages and discard source info messages that happen in the front
 * of the message queue, up to the first critical message.
 */

SUPRIVATE void *
suscli_analyzer_client_tx_thread_pre_cleanup(
  struct suscan_mq *mq,
  void *mq_user)
{
  struct suscli_analyzer_client_tx_thread_cleanup_ctx *ctx = NULL;

  SU_ALLOCATE_FAIL(ctx, struct suscli_analyzer_client_tx_thread_cleanup_ctx);

  ctx->mq = mq;

  return ctx;

fail:
  free(ctx);

  return NULL;
}

SUPRIVATE void
suscli_analyzer_client_tx_thread_cleanup_ctx_save_source_info(
  struct suscli_analyzer_client_tx_thread_cleanup_ctx *ctx,
  grow_buf_t *buffer)
{
  /* These are the first source info messages */
  if (ctx->head_source_info != NULL) {
    grow_buf_finalize(ctx->head_source_info);
    free(ctx->head_source_info);
  }

  ctx->head_source_info = buffer;
}

SUPRIVATE SUBOOL
suscli_analyzer_client_tx_thread_try_destroy(
  void *mq_user,
  void *cu_user,
  uint32_t type,
  void *data)
{
  struct suscli_analyzer_client_tx_thread_cleanup_ctx *ctx = cu_user;
  struct suscan_analyzer_remote_call call;
  uint32_t msg_type, msg_kind;
  grow_buf_t *buffer;

  suscan_analyzer_remote_call_init(&call, SUSCAN_ANALYZER_REMOTE_NONE);

  if (type == SUSCLI_ANALYZER_CLIENT_TX_MESSAGE) {
    buffer = data;

    /* Rewind */
    grow_buf_seek(buffer, 0, SEEK_SET);
    
    SU_TRY(suscan_analyzer_remote_call_deserialize_partial(&call, buffer));

    if (call.type == SUSCAN_ANALYZER_REMOTE_MESSAGE) {
      SU_TRY(
        suscan_analyzer_msg_deserialize_partial(
          &msg_type,
          buffer));
      
      switch (msg_type) {
        case SUSCAN_ANALYZER_MESSAGE_TYPE_SOURCE_INFO:
          if (!ctx->critical_reached) {
            suscli_analyzer_client_tx_thread_cleanup_ctx_save_source_info(
              ctx,
              buffer);
            ++ctx->discarded;
            return SU_TRUE;
          }
          break;

        /*
         * TODO: Maybe keep looped messages?
         */
        case SUSCAN_ANALYZER_MESSAGE_TYPE_PSD:
          grow_buf_finalize(buffer);
          free(buffer);
          ++ctx->discarded;
          return SU_TRUE;

        case SUSCAN_ANALYZER_MESSAGE_TYPE_INSPECTOR:
          /* Deserialize inspector message kind */
          SU_TRYZ(cbor_unpack_uint32(buffer, &msg_kind));

          /* Spectrum message. Discard */
          if (msg_kind == SUSCAN_ANALYZER_INSPECTOR_MSGKIND_SPECTRUM) {
            grow_buf_finalize(buffer);
            free(buffer);
            ++ctx->discarded;
            return SU_TRUE;
          }

          /* Other message. Assume critical */
          ctx->critical_reached = SU_TRUE;
          break;
        
        default:
          /* Other message. Assume critical */
          ctx->critical_reached = SU_TRUE;
          break;
      }
    } else {
      /* Not an analyzer message. Assume critical */
      ctx->critical_reached = SU_TRUE;
    }
  }

done:
  return SU_FALSE;
}

SUPRIVATE void
suscli_analyzer_client_tx_thread_post_cleanup(
  void *mq_user,
  void *cu_user)
{
  struct suscli_analyzer_client_tx_thread_cleanup_ctx *ctx = cu_user;

  if (ctx->head_source_info != NULL) {
    /* Give ownership away */
    suscan_mq_write_urgent_unsafe(
      ctx->mq,
      SUSCLI_ANALYZER_CLIENT_TX_MESSAGE,
      ctx->head_source_info);
  }

  SU_WARNING(
    "Slow network (%d PSD messages discarded)\n",
    ctx->discarded);
  
  free(ctx);
}

/* Initialization */
SUBOOL
suscli_analyzer_client_tx_thread_initialize(
    struct suscli_analyzer_client_tx_thread *self,
    int fd,
    unsigned int compress_threshold)
{
  struct suscan_mq_callbacks callbacks = 
  {
    NULL,
    suscli_analyzer_client_tx_thread_pre_cleanup,
    suscli_analyzer_client_tx_thread_try_destroy,
    suscli_analyzer_client_tx_thread_post_cleanup
  };

  SUBOOL ok = SU_FALSE;

  memset(self, 0, sizeof(struct suscli_analyzer_client_tx_thread));

  self->cancel_pipefd[0] = self->cancel_pipefd[1] = -1;
  self->fd = fd;
  self->compress_threshold = compress_threshold;

  SU_TRYCATCH(suscan_mq_init(&self->pool), goto done);
  self->pool_initialized = SU_TRUE;

  SU_TRYCATCH(suscan_mq_init(&self->queue), goto done);
  suscan_mq_set_callbacks(
    &self->queue,
    &callbacks);

  suscan_mq_set_cleanup_watermark(
    &self->queue,
    SUSCLI_ANALYZER_CLIENT_TX_CLEANUP_WATERMARK);

  self->queue_initialized = SU_TRUE;

  SU_TRYCATCH(pipe(self->cancel_pipefd) != -1, goto done);

  SU_TRYCATCH(
      pthread_create(
          &self->thread,
          NULL,
          suscli_analyzer_client_tx_thread_func,
          self) == 0,
      goto done);

  self->thread_running = SU_TRUE;

  ok = SU_TRUE;

done:
  if (!ok)
    suscli_analyzer_client_tx_thread_finalize(self);

  return ok;
}
