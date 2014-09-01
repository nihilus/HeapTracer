# by gera
#
# TODO:
# [/] Use VertexArrays to speed up drawing.
#   tried it, could not make it work and seem not faster
#     glEnableClientState(GL_VERTEX_ARRAY);
#     glEnableClientState(GL_COLOR_ARRAY);
#     glVertexPointer(3, GL_FLOAT, 0, pVertexArray);
#     glColorPointer(3, GL_FLOAT, 0, pColorArray);
#
#     glDrawArrays(GL_QUADS, 0, uiVertexCount);
#
#
# [ ] Progress Bar when loading a file

import wxversion
wxversion.select('2.8')
try:
    import psyco
    # psyco.log()
    psyco.full()
    pass
except:
    pass

from random import random
from time import time
from os import getcwd
from sys import argv

from OpenGL.GL import *
from OpenGL.GLU import gluUnProject
import wx
import wx.glcanvas

import numarray

# wx.DrawScroller !

MARGIN = 0.01

class Block:
  __totalBlocks = 0
  vertices = None

  def totalBlocks(self):
    return self.__totalBlocks

  def newOrder(self):
    answer = self.__class__.__totalBlocks = self.__class__.__totalBlocks + 1
    return answer

  def __init__(self, baseAddr, size = None, endAddr = None, openTime = None):
    if self.vertices is None:
        self.__class__.vertices = numarray.resize(0.0,(4,3))

    self.order = self.newOrder()
    if openTime is None:
        self.openTime = time()
    else:
        self.openTime = openTime
    self.closeTime = None

    self.baseAddr = baseAddr
    if size is None:
      if endAddr is None:
        raise Exception, 'Either size or endAddr must have a value'
      else:
        self.endAddr = endAddr
    else:
      self.endAddr = baseAddr + size

  def free(self, closeTime = None):
    if closeTime is None:
        self.closeTime = time()
    else:
        self.closeTime = closeTime

  def glColor(self, ratio):
    if (ratio < 0.5):
      r = 0.25 + 0.75*2 * ratio
      g = 0.75 + 0.25*2 * ratio
      b = 0.25 - 0.25*2 * ratio
    else:
      ratio = ratio*2 - 1
      r = 1.0
      g = 1.0 - 0.75 * ratio
      # b = 0.25 - 0.25 * ratio
      b = 0.0

    glColor(r, g, b)

    
  def dump(self):
    print "baseAddr: 0x%08x" % self.baseAddr
    print "endAddr: 0x%08x" % self.endAddr
    print "openTime: %f" % self.openTime
    print "closeTime: %r" % self.closeTime

  def draw(self, baseTime = 0, now = None, subPixelRender = False, names = False):
    left  = self.openTime - baseTime
    if self.closeTime is None:
      right = now - baseTime
    else:
      right = self.closeTime - baseTime
    top = self.baseAddr
    bottom = self.endAddr

    z = 1.0*self.order/self.totalBlocks()

    if names: glPushName(self.baseAddr / 4)
    self.glColor(z)
    # names[id(self)] = self

    glBegin(GL_QUADS);
    glVertex(left, top, z)
    glVertex(right, top, z)
    glVertex(right, bottom, z)
    glVertex(left, bottom, z)
    glEnd()
    """
    self.vertices[0][0] = left
    self.vertices[0][1] = top
    self.vertices[0][2] = z

    self.vertices[1][0] = right
    self.vertices[1][1] = top
    self.vertices[1][2] = z

    self.vertices[2][0] = right
    self.vertices[2][1] = bottom
    self.vertices[2][2] = z

    self.vertices[3][0] = left
    self.vertices[3][1] = bottom
    self.vertices[3][2] = z

    glVertexPointerf(self.vertices)
    glDrawArrays(GL_QUADS, 0, 4)
    """

    if names: glPopName()

    if subPixelRender:
        glBegin(GL_LINES)
        glVertex(left, top, z)
        glVertex(right, bottom, z)
        glEnd()

class Region:
  def __init__(self, baseAddr = None, size = None, endAddr = None, color = None):
    self.openTime = None
    self.closeTime = None
    self.lastTime = None

    if color is None:
      color = (0., 0., 0.5)

    self.color = color
    self.baseAddr = baseAddr
    if baseAddr and size:
      self.endAddr = baseAddr + size
    else:
      self.endAddr = endAddr

    self.autoBase = self.autoEnd = False
    if self.baseAddr is None: self.autoBase = True
    if self.endAddr is None: self.autoEnd = True

    self.openBlocks = {}	# malloced() and not free()
    self.closedBlocks = []	# already free() blocks

    self.changed = True
  
  def close(self, _time = None):
    if _time is None:
        self.closeTime = time()
    else:
        self.closeTime = _time
    self.changed = True

  def malloc(self, addr, size, _time = None):
    # if self.openBlocks.has_key(addr):
    #   raise Exception, "Blocks already allocated"
    if _time is None:
        self.lastTime = time()
    else:
        self.lastTime = _time

    if self.openTime is None:
        self.openTime = self.lastTime

    newBlock = self.openBlocks[addr] = Block(addr, size = size, openTime = _time)

    if self.autoBase and ((self.baseAddr is None) or (self.baseAddr > addr)):
      self.baseAddr = addr
    if self.autoEnd and ((self.endAddr is None) or (self.endAddr < newBlock.endAddr)):
      self.endAddr = newBlock.endAddr

    self.changed = True

  def realloc(self, oldAddr, newAddr, size, _time = None):
    self.free(oldAddr, _time)
    self.malloc(newAddr, size, _time)

  def free(self, addr, _time = None):
    if _time is None:
        self.lastTime = time()
    else:
        self.lastTime = _time

    try:
      block = self.openBlocks[addr]
      self.closedBlocks.append(block)
      block.free(_time)
      del self.openBlocks[addr]
    except:
      #if self.closedBlocks.has_key(addr):
      #  print "Double free detected! 0x%08x" % addr
      #else:
      # print "Freeing an unknown block! 0x%08x" % addr
      pass

    self.changed = True

  def draw(self, subPixelRender = False):
    # glBegin(GL_QUADS);
    for block in self.closedBlocks:
      block.draw(baseTime = self.openTime, subPixelRender = subPixelRender)

    for block in self.openBlocks:
      self.openBlocks[block].draw(baseTime = self.openTime, now = self.lastTime, subPixelRender = subPixelRender)
    # glEnd()

  def dump(self):
    print "baseAddr: 0x%08x" % self.baseAddr
    print "endAddr: 0x%08x" % self.endAddr
    print "openTime: %f" % self.openTime
    print "closeTime: %r" % self.closeTime

    print "\nCLOSE BLOCKS:"
    for block in self.closedBlocks:
      block.dump()

    print "\nOPEN BLOCKS:"
    for block in self.openBlocks:
      self.openBlocks[block].dump()

  def readFromFile(self, stream, progress = None):
    if stream.name.endswith('.ltrace'):
       # import threading
       # threading.Thread(target = self.readFromLTraceDump, args = (stream, progress)).start()
       self.readFromLTraceDump(stream, progress)
    else:
       self.malloc(0x8048000, 1000)
       self.malloc(0x8048500, 500)
       self.free(0x8048000)
       self.free(0x8048500)
       self.malloc(0x8048000, 500)
       self.malloc(0x8048000+500, 700)
       self.close()
       #self.dump()
    if progress:
       progress.Destroy()

  def readFromLTraceDump(self, stream, progress = None):
    # ltrace -o ls.ltrace -f -tt -i -efork,vfork,clone,malloc,realloc,free,calloc,mmap,munmap,mremap,mprotect,mmap2 /bin/ls

    # 13574 00:08:07.141541 [0x80532be] realloc(NULL, 312) = 0x8065300
    # 13574 00:08:07.141655 [0x8053307] malloc(1404)   = 0x8065440
    # 13574 00:08:07.141998 [0x804a4d3] free(0x8061ef0) = <void>

    t = time()
    while 1:
      line = stream.readline()[:-1]
      if progress:
          (keepGoing, skip) = progress.Update(stream.tell())
          if not keepGoing: break

      if not line:
        break

      try:
          pid, _time, caller, call = line.split(None, 3)
          _time = [float(x) for x in _time.split(':')]
          _time = _time[0]*3600+_time[1]*60+_time[2]
          call, result = call.split('=')
          call, args = call.split('(', 1)
          args = args.split(')', 1)[0]
          args = args.split(', ')

          pid = long(pid)
          caller = long(caller[3:-1], 16)
          result = result.strip()

      except Exception:
          print 'Skipping line: "%r"' % line

      if   'malloc' == call:
        self.malloc(long(result[2:],16), long(args[0]), _time)
      elif 'free' == call:
        if args[0] == 'NULL':
          arg = 0
        else:
          arg = long(args[0][2:], 16)
        self.free(arg, _time)
      elif 'realloc' == call:
        if args[0] == 'NULL':
          oldAddr = 0
        else:
          oldAddr = long(args[0][2:],16)
        size = long(args[1])
        result = long(result[2:],16)
        self.realloc(oldAddr, result, size, _time)
    print "Time loading file: %f" % (time()-t)

  def readFromTrussDump(self, stream):
    # truss -o ls.truss -f -d -l -t \!all -u a.out,*::malloc,realloc,free,calloc,fork,vfork,mmap,munmap,mprotect,brk /bin/ls
    pass

  def startListening(self, port = 12345, SetStatusText = None):
    # this will listen on a TCP port
    # the protocol is simple:
    # malloc 0x12345678 1234 (address size)
    # free 0x12345678 (address in hex)
    # quit
    # close
    import socket
    sock = socket.socket()
    sock.bind(('', port))
    sock.listen(1)
    if SetStatusText:
      SetStatusText("Listening on %s:%d" % (sock.getsockname()))
    while 1:
      client, address = sock.accept()
      if SetStatusText:
        SetStatusText("Received connection from %s:%d" % address)
      client = client.makefile()
      while 1:
        line = client.readline()+' '
        call, args = line.split(None,1)
        if   'malloc' == call:
          addr, size = args.split()
          addr = long(addr[2:], 16)
          size = long(size)
          self.malloc(addr, size)
        elif 'free' == call:
          addr = long(args[2:], 16)
          self.free(addr)
        elif 'quit' == call:
          return
        elif 'close' == call:
          client.close()
          break
    
class OpenGLCanvas(wx.glcanvas.GLCanvas):
    def __init__(self, parent, **kargs):
        wx.glcanvas.GLCanvas.__init__(self, parent, -1, **kargs)
        self.SetStatusText = parent.SetStatusText
        self.init = False
        # initial mouse position
        self.size = None
        self.Bind(wx.EVT_ERASE_BACKGROUND, self.OnEraseBackground)
        self.Bind(wx.EVT_SIZE, self.OnSize)
        self.Bind(wx.EVT_PAINT, self.OnPaint)
        self.Bind(wx.EVT_LEFT_DOWN, self.OnMouseLeftDown)
        self.Bind(wx.EVT_LEFT_UP, self.OnMouseLeftUp)
        self.Bind(wx.EVT_RIGHT_DOWN, self.OnMouseRightDown)
        self.Bind(wx.EVT_MOTION, self.OnMouseMotion)
        self.Bind(wx.EVT_CHAR, self.OnKeyDown)
        self.Bind(wx.EVT_IDLE, self.onIdle)

        self.root = None
        self.x = self.y = self.startx = self.starty = None
        self.zoom = []
        self.listIndex = None
        self.subPixelRender = False

    def OnEraseBackground(self, event):
        # Do nothing, to avoid flashing on MSW.
        pass 

    def OnSize(self, event):
        size = self.size = self.GetClientSize()
        if self.GetContext():
            self.SetCurrent()
            glViewport(0, 0, size.width, size.height)
        event.Skip()

    def OnPaint(self, event):
        dc = wx.PaintDC(self)
        self.SetCurrent()
        if not self.init:
            self.InitGL()
            self.init = True
        self.OnDraw()

    def OnMouseLeftDown(self, evt):
        self.CaptureMouse()
        self.x, self.y = self.startx, self.starty = evt.GetPosition()

    def OnMouseLeftUp(self, evt):
        t_left, t_top, z = gluUnProject(self.startx, self.size[1] - self.starty, 0)
        right, bottom, z = gluUnProject(self.x, self.size[1] - self.y, 0)

        left = min(t_left, right)
        right = max(t_left, right)
        top = min(t_top, bottom)
        bottom = max(t_top, bottom)

        self.zoom.append((left, right, top, bottom))
        self.x = self.y = self.startx = self.starty = None
        self.ReleaseMouse()

        self.Refresh(False)

    def OnMouseMotion(self, evt):
        x, y = evt.GetPosition()
        if not evt.RightIsDown():
            self.SetStatusText('Time: %f Address: 0x%08x' % self.screenToReal(x, y))
        if evt.Dragging() and evt.LeftIsDown():
            self.x, self.y = x, y
            self.Refresh(False)

    def select(self):
      glCallList(self.listIndex)

    def OnMouseRightDown(self, evt):
      x, y = evt.GetPosition()
      selection = glSelectWithCallback(x, y, self.select, 1, 1)
      selection = selection[-1][2][0]*4
      self.SetStatusText("Selected: 0x%08x" % (selection))

    def OnKeyDown(self, evt):
        if evt.GetKeyCode() == wx.WXK_BACK:
            self.zoom.pop()
            self.Refresh(False)

    # custom methods start here

    def InitGL(self):
        # set viewing projection
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()

        # position viewer
        # glMatrixMode(GL_MODELVIEW)
        # glTranslate(0.0, 0.0, -2.0)

        # position object
        #glRotate(self.y, 1.0, 0.0, 0.0)
        #glRotate(self.x, 0.0, 1.0, 0.0)

        glEnable(GL_DEPTH_TEST)
        # glEnableClientState(GL_VERTEX_ARRAY)
        glClearColor(0.,0.,0.,1.)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        #glEnable(GL_LIGHTING)
        #glEnable(GL_LIGHT0)
 
    def screenToReal(self, x, y):
        x, y, z = gluUnProject(x, self.size[1] - y, 0)
        return x,y

    def drawSelectionBox(self):
        if self.x is None:
          return

        left, top = self.screenToReal(self.startx, self.starty)
        right, bottom = self.screenToReal(self.x, self.y)

        z = 1.5

        glEnable(GL_BLEND);
        glColor(0., 0., 1., 0.5)

        glBegin(GL_QUADS);
        glVertex(left, top, z)
        glVertex(right, top, z)
        glVertex(right, bottom, z)
        glVertex(left, bottom, z)
        glEnd()
        glDisable(GL_BLEND);

    def OnDraw(self):
        if self.size is None:
            self.size = self.GetClientSize()

        if self.listIndex is None: return
        # clear color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        if self.zoom:
            left, right, bottom, top = self.zoom[-1]
        else:
            left = 0
            right = self.root.lastTime - self.root.openTime
            top = self.root.endAddr
            bottom = self.root.baseAddr
            if top is None: top = -1
            if bottom is None: bottom = 1

        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        glTranslate(0.0, 0.0, -2.0)

        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()

        glOrtho(left, right, top, bottom, -2, 2)

        glCallList(self.listIndex)
        # glPrint(status)
        self.drawSelectionBox()

        self.SwapBuffers()
        
    def onIdle(self, evt):
        self.compile()
        self.Refresh(False)

    def compile(self):
        # print '.',
        if not self.root or not self.root.changed:
            return

        self.root.changed = False

        t = time()

        if self.root is None: return
        self.listIndex = glGenLists(1)
        glNewList(self.listIndex, GL_COMPILE)
        self.root.draw(subPixelRender = self.subPixelRender)
        glEndList()
        
        print "Compile time: %f" % (time()-t)

    def switchSubPixelRender(self):
        self.subPixelRender = not self.subPixelRender
        self.root.changed = True
        self.compile()

class HeapDraw(wx.App):
    def OnInit(self):
        windowXpixels = 8
        windowYpixels = 27 
        self.frame = wx.Frame(None, -1, "HeapDraw") #,
        
        # menu
        self.mainMenu = self.buildMenu()
        self.frame.SetMenuBar(self.mainMenu)

        # status bar
        self.frame.CreateStatusBar()
        self.frame.SetStatusText('welcome')

        # toolbar
        self.buildToolBar()

        # OpenGL canvas
        self.graph = OpenGLCanvas(self.frame)

        # show
        self.frame.Show(True)
        self.SetTopWindow(self.frame)
        return True

    def buildMenu(self):
        menu = wx.MenuBar()
        fileMenu = wx.Menu()
        menu.Append(fileMenu, '&File')

        open_id = wx.NewId()
        save_id = wx.NewId()
        exit_id = wx.NewId()
        listen_id = wx.NewId()

        fileMenu.Append(open_id, '&Open\tCtrl-O', 'Open a file')
        fileMenu.Append(save_id, '&Save\tCtrl-S', 'Save to a file').Enable(False)
        fileMenu.Append(listen_id, 'Start &listening\tCtrl-L', 'Start listening on a TCP port')
        fileMenu.Append(exit_id, 'E&xit\tAlt-X')

        self.Bind(wx.EVT_MENU, self.OpenFile, id=open_id)
        self.Bind(wx.EVT_MENU, self.CloseWindow, id=exit_id)
        self.Bind(wx.EVT_MENU, self.StartListening, id=listen_id)
        return menu

    def buildToolBar(self):
        tb = self.frame.CreateToolBar(wx.TB_HORIZONTAL | wx.NO_BORDER)
        tsize = (24,24)
        subpixel_bmp = wx.ArtProvider.GetBitmap(wx.ART_FIND, wx.ART_TOOLBAR, tsize)

        tb.SetToolBitmapSize(tsize)
        subpixel_id = wx.NewId()
        tb.AddLabelTool(subpixel_id, 'Subpixel', subpixel_bmp, shortHelp = 'Switch SubPixel Rendering', longHelp = 'Turn On/Off rendering of blocks which are smaller than a pixel. It introduces some noice in the graph, but show more information')

        self.Bind(wx.EVT_TOOL, self.OnSubPixelRender, id=subpixel_id)

    def OnSubPixelRender(self, event):
        self.graph.switchSubPixelRender()
        self.graph.Refresh(False)

    def CloseWindow(self, event):
        self.frame.Close()

    def OpenFile(self, event):
        dlg = wx.FileDialog(
            self.frame, message="Choose a file",
            defaultDir=getcwd(), 
            defaultFile="",
            wildcard="ltrace output (.ltrace)|*.ltrace|truss output (.truss)|*.truss|All Files|*",
            style=wx.OPEN | wx.CHANGE_DIR
            )
        if dlg.ShowModal() == wx.ID_OK:
            self.loadFile(dlg.GetPaths()[0])
        dlg.Destroy()

    def loadFile(self, fileName):
        stream = file(fileName)

        stream.seek(0,2)
        max = stream.tell()
        stream.seek(0,0)
        progress = None
        """
        progress = wx.ProgressDialog("Loading file...",
                           "It may take some time",
                           maximum = max,
                           parent = self.frame,
                           style = wx.PD_CAN_ABORT
                            # | wx.PD_APP_MODAL
                            | wx.PD_ELAPSED_TIME
                            | wx.PD_ESTIMATED_TIME
                            | wx.PD_REMAINING_TIME
                            )
        """
        newRoot = Region()
        newRoot.readFromFile(stream, progress)
        self.graph.root = newRoot
        self.graph.Refresh(True)

    def StartListening(self, event):
        import threading
        newRoot = Region()
        threading.Thread(target = newRoot.startListening, args = (12345, self.frame.SetStatusText)).start()
        self.graph.root = newRoot

if __name__ == '__main__':
  app = HeapDraw(0)
  if len(argv)>1:
      app.loadFile(argv[1])
  app.MainLoop()

