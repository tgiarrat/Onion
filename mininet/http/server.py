# python -m SimpleHTTPPutServer 8080
import SimpleHTTPServer
import BaseHTTPServer
import os

class SputHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def do_PUT(self):
        length = int(self.headers["Content-Length"])
        path = self.translate_path("keys/"+self.client_address[0])
        print self.client_address[0]
        with open(path, "wb") as dst:
            dst.write(self.rfile.read(length))
            #with open("keys/list.txt", "a") as dst:
            #    dst.write(self.client_address[0] +"\n")

    def do_GET(self):
        """Serve a GET request."""
        f = self.send_head()
        if self.path == '/list':
            files = os.listdir('keys')
            for fileName in files:
                if (fileName != self.client_address[0]):
                    self.wfile.write(fileName+'\n')
            if f:
                f.close()
        elif f:
            self.copyfile(f, self.wfile)
            f.close()


if __name__ == '__main__':
    SimpleHTTPServer.test(HandlerClass=SputHTTPRequestHandler)
