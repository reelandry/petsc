import script

class Compiler(script.Script):
  '''The SIDL compiler'''
  def __init__(self, argDB):
    script.Script.__init__(self, argDB = argDB)
    self.language        = 'SIDL'
    self.sourceExtension = '.sidl'
    self.clients         = []
    self.clientDirs      = {}
    self.servers         = []
    self.serverDirs      = {}
    self.includes        = []
    return

  def __getstate__(self):
    '''We do not want to pickle Scandal'''
    d = script.Script.__getstate__(self)
    if 'scandal' in d:
      del d['scandal']
    return d

  def __call__(self, source, target = None):
    '''This will compile the SIDL source'''
    outputFiles             = {}
    self.scandal.includes   = self.includes
    self.scandal.targets    = source
    self.scandal.clients    = self.clients
    self.scandal.clientDirs = self.clientDirs
    self.scandal.servers    = []
    self.scandal.serverDirs = {}
    self.scandal.run()
    for lang in self.scandal.outputFiles:
      outputFiles['Client '+lang] = self.scandal.outputFiles[lang]
    self.scandal.clients    = []
    self.scandal.clientDirs = {}
    self.scandal.servers    = self.servers
    self.scandal.serverDirs = self.serverDirs
    self.scandal.run()
    for lang in self.scandal.outputFiles:
      outputFiles['Server '+lang] = self.scandal.outputFiles[lang]
    return ('', '', 0, outputFiles)

  def checkSetup(self):
    '''Check that this module has been specified. We assume that configure has checked its viability.'''
    import os

    if not hasattr(self, 'scandal'):
      self.scandal = self.getModule(os.path.join(self.argDB['ASE_DIR'], 'driver', 'python'), 'scandal').Scandal(argDB = self.argDB)
      self.scandal.setup()
    return self.scandal

  def getTarget(self, source):
    '''Returns the default target for the given source file, or None'''
    return None
