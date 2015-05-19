// Copyright 2010-2013, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <Cocoa/Cocoa.h>

#include "base/base.h"
#include "base/logging.h"
#include "net/http_client_common.h"
#include "net/http_client_mac.h"

@interface HTTPStream : NSObject{
@private
  const NSURLRequest  *request_;
  string              *output_string_;
  size_t              max_data_size_;
  size_t              output_size_;
  bool                include_header_;
  int                 status_code_;
  bool                finished_;
  bool                error_flag_;
}
@end
@implementation HTTPStream : NSObject

- (HTTPStream *)initWithRequest:(const NSURLRequest *)request
                   outputString:(string *)output_string
                    maxDataSize:(size_t)max_data_size
                 include_header:(bool)include_header {
  self = [super init];
  if (self == nil) {
    return nil;
  }
  if (NULL != output_string_) {
    output_string_->clear();
  }
  request_ = request;
  output_string_ = output_string;
  max_data_size_ = max_data_size;
  output_size_ = 0;
  include_header_ = include_header;

  finished_ = false;
  error_flag_ = false;

  return self;
}

// Append data to output
- (int)append:(const void *)buf size:(size_t)size {
  if (output_size_ + size >= max_data_size_) {
    size = (max_data_size_ - output_size_);
    LOG(WARNING) << "too long data max_data_size=" << max_data_size_;
  }

  if (output_string_ != NULL) {
    VLOG(2) << "Recived: " << size << " bytes to std::string";
    output_string_->append((const char*)buf, size);
  }

  output_size_ += size;
  return size;
}

// Create HTTP response header data from NSURLResponse
// Because NSHTTPURLResponse doesn't provide access to raw header data,
// the return value is not the same data as the server respond.
- (NSData *)dataOfResponseHeader:(NSURLResponse *)response {
  NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
  NSDictionary *dictionary = [httpResponse allHeaderFields];
  NSMutableData *headerData = [NSMutableData dataWithCapacity:1024];

  // TODO(horo): This HTTP status line is fake.
  //             NSHTTPURLResponse doesn't provide access to raw header data.
  NSString *statusLine = [NSString stringWithFormat:@"HTTP/1.1 %d OK\r\n",
                           status_code_];
  [headerData appendData:[statusLine dataUsingEncoding:NSUTF8StringEncoding]];

  // Enumrate header fields
  NSArray *keys = [dictionary allKeys];
  for (int i = 0; i < [keys count]; ++i) {
    NSString *line = [NSString stringWithFormat:@"%@: %@\r\n",
                       [keys objectAtIndex:i],
                       [dictionary objectForKey:[keys objectAtIndex:i]]];
    [headerData appendData:[line dataUsingEncoding:NSUTF8StringEncoding]];
  }
  [headerData appendBytes:"\r\n" length:2];
  return headerData;
}

// connection:didReceiveResponse:
//  Sent when the connection has received sufficient data
//  to construct the URL response for its request.
- (void)connection:(NSURLConnection *)connection
  didReceiveResponse:(NSURLResponse *)response {
  NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
  status_code_ = [httpResponse statusCode];
  if (include_header_) {
    NSData *headerData = [self dataOfResponseHeader:response];
    int size = [headerData length];
    if([self append:[headerData bytes] size:size] != size) {
      [connection cancel];
      finished_ = true;
      error_flag_ = true;
    }
  }
}

// connectionDidFinishLoading:
//  Sent when a connection has finished loading successfully.
- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
  finished_ = true;
}

// connection:didFailWithError:
//   Sent when a connection fails to load its request successfully.
- (void)connection:(NSURLConnection *)connection
  didFailWithError:(NSError *)error {
  LOG(ERROR) << "HTTPStream failed with error: "
             << [[[request_ URL] absoluteString] UTF8String] << " ["
             << [[error localizedDescription] UTF8String] << "]";
  finished_ = true;
  error_flag_ = true;
}

// connection:didReceiveData:
//  Sent as a connection loads data incrementally.
- (void)connection:(NSURLConnection *)connection
    didReceiveData:(NSData *)data {
  const char *buf = (const char *)[data bytes];
  int size = [data length];
  if (size != [self append:buf size:size]) {
    [connection cancel];
    error_flag_ = true;
    finished_ = true;
  }
}

// connection:willSendRequest:redirectResponse:
//  Sent when the connection determines that it must change URLs
//  in order to continue loading a request.
- (NSURLRequest *)connection:(NSURLConnection *)connection
             willSendRequest:(NSURLRequest *)request
            redirectResponse:(NSURLResponse *)redirectResponse {
  if (!redirectResponse) {
    return request;
  }
  if (!include_header_) {
    return request;
  }
  NSData *headerData = [self dataOfResponseHeader:redirectResponse];
  int size = [headerData length];
  if([self append:[headerData bytes] size:size] != size) {
    [connection cancel];
    finished_ = true;
    error_flag_ = true;
  }
  return request;
}

// This method is called in a new thread which is created by "start" method.
- (void)sendRequestThread:(NSURLRequest *)request {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [[[NSURLConnection alloc] initWithRequest:request delegate:self] autorelease];

  // wait aun run runloop until HTTP connection will be finished
  while (!finished_) {
    [[NSRunLoop currentRunLoop]
      runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01f]];
  }
  [pool drain];
}

// Start HTTP session and return result.
- (bool)start {
  NSThread *thread = [[NSThread alloc] initWithTarget:self
                        selector:@selector(sendRequestThread:)
                        object:request_];
  [thread start];

  // wait until thread finishes
  while (NO == [thread isFinished]) {
    [NSThread sleepForTimeInterval:0.01f];
  }
  [thread cancel];
  [thread release];

  return (status_code_ == mozc::kOKResponseCode) && (!error_flag_);
}
@end

namespace mozc {
bool MacHTTPRequestHandler::Request(HTTPMethodType type,
                                    const string &url,
                                    const char *post_data,
                                    size_t post_size,
                                    const HTTPClient::Option &option,
                                    string *output_string) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSString *urlStr = [NSString stringWithUTF8String:url.c_str()];

  NSMutableURLRequest *request;
  request = [[[NSMutableURLRequest alloc] init] autorelease];
  [request setHTTPShouldHandleCookies:NO];
  [request setURL:[NSURL URLWithString:urlStr]];
  [request setValue:[NSString stringWithUTF8String:kUserAgent]
    forHTTPHeaderField: @"User-Agent"];
  [request setCachePolicy:NSURLRequestReloadIgnoringLocalAndRemoteCacheData];
  [request setTimeoutInterval:option.timeout];

  // The default HTTP method is “GET”.
  if (type == HTTP_POST) {
    [request setHTTPMethod:@"POST"];
    if (post_data != NULL) {
      // The framework will automatically add the content type and
      // content length, but we put it explicitly just in case.
      NSString *length = [NSString stringWithFormat:@"%zd", post_size];
      [request setValue:length forHTTPHeaderField:@"Content-Length"];
      [request setValue:@"application/x-www-form-urlencoded"
      forHTTPHeaderField: @"Content-Type"];

      [request setHTTPBody:[NSData dataWithBytes:post_data length:post_size]];
    }
  } else if (type == HTTP_HEAD) {
    [request setHTTPMethod:@"HEAD"];
  }

  // Add http header fields
  for (size_t i = 0; i < option.headers.size(); ++i) {
    const string::size_type pos = option.headers[i].find(": ");
    if (pos != string::npos) {
      const string key = option.headers[i].substr(0, pos);
      const string value = option.headers[i].substr(pos + 2);
      [request setValue:[NSString stringWithUTF8String:value.c_str()]
        forHTTPHeaderField:[NSString stringWithUTF8String:key.c_str()]];
    }
  }

  HTTPStream *stream = [[[HTTPStream alloc] initWithRequest:request
                          outputString:output_string
                          maxDataSize:option.max_data_size
                          include_header:option.include_header]
                        autorelease];
  const bool ret = [stream start];

  [pool drain];

  return ret;
}
}  // namespace mozc
