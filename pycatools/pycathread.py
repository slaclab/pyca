import pyca
from threading import Thread

class PycaThread(Thread):
  def __init__(self, group=None,target=None,name=None,args=(),kwargs={}):
    Thread.__init__(self, group,target,name,args,kwargs)
    pass
  
  def run(self):
    pyca.attach_context()
    Thread.run(self)
    pass

  pass # PycaThread
