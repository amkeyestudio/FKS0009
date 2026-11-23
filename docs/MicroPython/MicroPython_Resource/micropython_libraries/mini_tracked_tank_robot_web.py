# mini_tracked_tank_robot_web.py - 极简 HTTP 服务器（非阻塞）
# 只做本套教程需要的三件事：发网页、收参数、回文本
# 用法：
#   server = WebServer()
#   server.route('/', index)            # 访问首页时调用 index()
#   server.route('/cmd', on_cmd)        # 访问 /cmd?d=f 时调用 on_cmd({'d': 'f'})
#   while True:
#       server.handle()                 # 每轮处理一个请求，没请求立刻返回

import socket


def _unquote(text):
    # 把 URL 里的 %XX 和 + 还原成正常字符（按 UTF-8 解码）
    if '%' not in text and '+' not in text:
        return text
    text = text.replace('+', ' ')
    buf = bytearray()
    i = 0
    while i < len(text):
        c = text[i]
        if c == '%' and i + 2 < len(text):
            try:
                buf.append(int(text[i + 1:i + 3], 16))
                i += 3
                continue
            except ValueError:
                pass
        buf.extend(c.encode('utf-8'))
        i += 1
    return buf.decode('utf-8', 'ignore')


class WebServer:

    def __init__(self, port=80):
        self.routes = {}
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('', port))
        self.sock.listen(5)
        self.sock.setblocking(False)     # accept 不阻塞，主循环才能同时跑车

    def route(self, path, handler):
        # handler(params) 的返回值作为响应正文
        self.routes[path] = handler

    def handle(self):
        conn = None
        try:
            conn, _ = self.sock.accept()
        except OSError:
            return                        # 没有客户端排队，直接返回

        try:
            conn.settimeout(0.2)
            request = conn.recv(1024).decode('utf-8', 'ignore')
            first = request.split('\r\n')[0]
            parts = first.split(' ')
            target = parts[1] if len(parts) >= 2 else '/'

            path, _, query = target.partition('?')
            params = {}
            for item in query.split('&'):
                if '=' in item:
                    k, v = item.split('=', 1)
                    params[_unquote(k)] = _unquote(v)

            handler = self.routes.get(path)
            if handler is None:
                body = '404 Not Found'
                ctype = 'text/plain; charset=utf-8'
            else:
                body = handler(params)
                if body is None:
                    body = 'OK'
                if path in ('/', '/index.html'):
                    ctype = 'text/html; charset=utf-8'
                else:
                    ctype = 'text/plain; charset=utf-8'

            if isinstance(body, str):
                body = body.encode('utf-8')

            header = ('HTTP/1.1 200 OK\r\n'
                      'Content-Type: %s\r\n'
                      'Connection: close\r\n\r\n' % ctype)
            conn.send(header.encode('utf-8') + body)
        except Exception as e:
            print('HTTP 异常：', e)
        finally:
            if conn is not None:
                try:
                    conn.close()
                except Exception:
                    pass
