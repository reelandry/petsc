import script

import os

class Make(script.Script):
  '''Template for individual project makefiles. All project makes start with a local RDict.'''
  def __init__(self, builder = None):
    import RDict
    import config.framework
    import sys

    script.Script.__init__(self, sys.argv[1:], RDict.RDict())
    if builder is None:
      self.framework = config.framework.Framework(self.clArgs+['-noOutput'], self.argDB)
      self.builder   = __import__('builder').Builder(self.framework)
    else:
      self.builder   = builder
      self.framework = builder.framework
    self.builder.pushLanguage('C')
    return

  def setupHelp(self, help):
    import nargs

    help = script.Script.setupHelp(self, help)
    help.addArgument('Make', 'forceConfigure', nargs.ArgBool(None, 0, 'Force a reconfiguration', isTemporary = 1))
    return help

  def setupDependencies(self, sourceDB):
    '''Override this method to setup dependencies between source files'''
    return

  def setup(self):
    script.Script.setup(self)
    self.builder.setup()
    self.setupDependencies(self.builder.shouldCompile.sourceDB)
    return

  def shouldConfigure(self, builder, framework):
    '''Determine whether we should reconfigure
       - If the configure header or substitution files are missing
       - If -forceConfigure is given
       - If configure.py has changed
       - If the database does not contain a cached configure'''
    if not os.path.isfile(framework.header) or not reduce(lambda x,y: x and y, [os.path.isfile(pair[1]) for pair in framework.substFiles], True):
      self.logPrint('Reconfiguring due to absence of configure generated files')
      return 1
    if self.argDB['forceConfigure']:
      self.logPrint('Reconfiguring forced')
      return 1
    if (not 'configure.py' in self.builder.shouldCompile.sourceDB or
        not self.builder.shouldCompile.sourceDB['configure.py'][0] == self.builder.shouldCompile.sourceDB.getChecksum('configure.py')):
      self.logPrint('Reconfiguring due to changed configure.py')
      return 1
    if not 'configureCache' in self.argDB:
      self.logPrint('Reconfiguring due to absence of configure cache')
      return 1
    return 0

  def setupConfigure(self, framework):
    framework.header = os.path.join('include', 'config.h')
    try:
      framework.addChild(self.getModule(self.getRoot(), 'configure').Configure(framework))
    except ImportError, e:
      self.logPrint('Configure module not present: '+str(e))
      return 0
    return 1

  def configure(self, builder):
    '''Run configure if necessary and return the configuration Framework'''
    import cPickle

    if not self.setupConfigure(self.framework):
      return
    doConfigure = self.shouldConfigure(builder, self.framework)
    if not doConfigure:
      try:
        cache          = self.argDB['configureCache']
        self.framework = cPickle.loads(cache)
        self.framework.setArgDB(self.argDB)
        self.logPrint('Loaded configure to cache: size '+str(len(cache)))
      except cPickle.UnpicklingError, e:
        doConfigure    = 1
        self.logPrint('Invalid cached configure: '+str(e))
    if doConfigure:
      self.logPrint('Starting new configuration')
      self.framework.configure()
      self.builder.shouldCompile.sourceDB.updateSource('configure.py')
      cache = cPickle.dumps(self.framework)
      self.argDB['configureCache'] = cache
      self.logPrint('Wrote configure to cache: size '+str(len(cache)))
    else:
      self.logPrint('Using cached configure')
      self.framework.cleanup()
    return self.framework

  def updateDependencies(self, sourceDB):
    '''Override this method to update dependencies between source files. This method saves the database'''
    sourceDB.save()
    return

  def build(self, builder):
    '''Override this method to execute all build operations. This method does nothing.'''
    return

  def run(self):
    self.setup()
    self.logPrint('Starting Build', debugSection = 'build')
    self.configure(self.builder)
    self.build(self.builder)
    self.updateDependencies(self.builder.shouldCompile.sourceDB)
    self.logPrint('Ending Build', debugSection = 'build')
    return 1

class SIDLMake(Make):
  def getSidl(self):
    if not hasattr(self, '_sidl'):
      self._sidl = [os.path.join(self.getRoot(), 'sidl', f) for f in filter(lambda s: os.path.splitext(s)[1] == '.sidl', os.listdir(os.path.join(self.getRoot(), 'sidl')))]
    return self._sidl
  def setSidl(self, sidl):
    self._sidl = sidl
    return
  sidl = property(getSidl, setSidl, doc = 'The list of input SIDL files')

  def getIncludes(self):
    if not hasattr(self, '_includes'):
      self._includes = []
    return self._includes
  def setIncludes(self, includes):
    self._includes = includes
    return
  includes = property(getIncludes, setIncludes, doc = 'The list of SIDL include files')

  def getClientLanguages(self):
    if not hasattr(self, '_clientLanguages'):
      self._clientLanguages = ['Python']
    return self._clientLanguages
  def setClientLanguages(self, clientLanguages):
    self._clientLanguages = clientLanguages
    return
  clientLanguages = property(getClientLanguages, setClientLanguages, doc = 'The list of client languages')

  def getServerLanguages(self):
    if not hasattr(self, '_serverLanguages'):
      self._serverLanguages = ['Python']
    return self._serverLanguages
  def setServerLanguages(self, serverLanguages):
    self._serverLanguages = serverLanguages
    return
  serverLanguages = property(getServerLanguages, setServerLanguages, doc = 'The list of server languages')

  def setupConfigure(self, framework):
    framework.require('config.libraries', None)
    framework.require('config.python', None)
    return Make.setupConfigure(self, framework)

  def configure(self, builder):
    framework = Make.configure(self, builder)
    self.libraries = framework.require('config.libraries', None)
    self.python    = framework.require('config.python', None)
    return framework

  def setupSIDL(self, builder, sidlFile):
    baseName = os.path.splitext(os.path.basename(sidlFile))[0]
    builder.loadConfiguration('SIDL '+baseName)
    builder.pushConfiguration('SIDL '+baseName)
    builder.pushLanguage('SIDL')
    compiler            = builder.getCompilerObject()
    compiler.clients    = self.clientLanguages
    compiler.clientDirs = dict([(lang, 'client-'+lang.lower()) for lang in self.clientLanguages])
    compiler.servers    = self.serverLanguages
    compiler.serverDirs = dict([(lang, 'server-'+lang.lower()+'-'+baseName) for lang in self.serverLanguages])
    compiler.includes   = self.includes
    builder.popLanguage()
    builder.popConfiguration()
    return

  def setupPythonClient(self, builder, sidlFile, language):
    baseName  = os.path.splitext(os.path.basename(sidlFile))[0]
    builder.pushConfiguration('SIDL '+baseName)
    builder.pushLanguage('SIDL')
    clientDir = builder.getCompilerObject().clientDirs[language]
    builder.popLanguage()
    builder.popConfiguration()
    builder.loadConfiguration(language+' Stub '+baseName)
    builder.pushConfiguration(language+' Stub '+baseName)
    builder.getCompilerObject().includeDirectories.extend(self.python.include)
    builder.getCompilerObject().includeDirectories.append(clientDir)
    builder.getLinkerObject().libraries.extend(self.python.lib)
    builder.popConfiguration()
    return

  def setupPythonIOR(self, builder, sidlFile, language):
    baseName  = os.path.splitext(os.path.basename(sidlFile))[0]
    builder.pushConfiguration('SIDL '+baseName)
    builder.pushLanguage('SIDL')
    serverDir = builder.getCompilerObject().serverDirs[language]
    builder.popLanguage()
    builder.popConfiguration()
    builder.loadConfiguration(language+' IOR '+baseName)
    builder.pushConfiguration(language+' IOR '+baseName)
    builder.getCompilerObject().includeDirectories.append(serverDir)
    builder.popConfiguration()
    return

  def setupPythonSkeleton(self, builder, sidlFile, language):
    baseName  = os.path.splitext(os.path.basename(sidlFile))[0]
    builder.pushConfiguration('SIDL '+baseName)
    builder.pushLanguage('SIDL')
    serverDir = builder.getCompilerObject().serverDirs[language]
    builder.popLanguage()
    builder.popConfiguration()
    builder.loadConfiguration(language+' Skeleton '+baseName)
    builder.pushConfiguration(language+' Skeleton '+baseName)
    builder.getCompilerObject().includeDirectories.extend(self.python.include)
    builder.getCompilerObject().includeDirectories.append(serverDir)
    builder.getLinkerObject().libraries.extend(self.python.lib)
    builder.popConfiguration()
    return

  def setupPythonServer(self, builder, sidlFile, language):
    baseName = os.path.splitext(os.path.basename(sidlFile))[0]
    self.setupPythonIOR(builder, sidlFile, language)
    self.setupPythonSkeleton(builder, sidlFile, language)
    builder.loadConfiguration(language+' Server '+baseName)
    builder.pushConfiguration(language+' Server '+baseName)
    builder.getLinkerObject().libraries.extend(self.python.lib)
    builder.popConfiguration()
    return

  def buildSIDL(self, builder, sidlFile):
    baseName = os.path.splitext(os.path.basename(sidlFile))[0]
    config   = builder.pushConfiguration('SIDL '+baseName)
    builder.pushLanguage('SIDL')
    builder.compile([sidlFile])
    builder.popLanguage()
    builder.popConfiguration()
    builder.saveConfiguration('SIDL '+baseName)
    return config.outputFiles

  def buildPythonClient(self, builder, sidlFile, language, generatedSource):
    baseName = os.path.splitext(os.path.basename(sidlFile))[0]
    config   = builder.pushConfiguration(language+' Stub '+baseName)
    for f in generatedSource['Client '+language]['Cxx']:
      builder.compile([f])
      builder.link([builder.getCompilerTarget(f)], shared = 1)
    builder.popConfiguration()
    builder.saveConfiguration(language+' Stub '+baseName)
    if 'Linked ELF' in config.outputFiles:
      return config.outputFiles['Linked ELF']
    return []

  def buildPythonIOR(self, builder, sidlFile, language, generatedSource):
    baseName = os.path.splitext(os.path.basename(sidlFile))[0]
    config   = builder.pushConfiguration(language+' IOR '+baseName)
    for f in generatedSource:
      builder.compile([f])
    builder.popConfiguration()
    builder.saveConfiguration(language+' IOR '+baseName)
    if 'ELF' in config.outputFiles:
      return config.outputFiles['ELF']
    return []

  def buildPythonSkeletons(self, builder, sidlFile, language, generatedSource):
    baseName = os.path.splitext(os.path.basename(sidlFile))[0]
    config   = builder.pushConfiguration(language+' Skeleton '+baseName)
    for f in generatedSource:
      builder.compile([f])
    builder.popConfiguration()
    builder.saveConfiguration(language+' Skeleton '+baseName)
    if 'ELF' in config.outputFiles:
      return config.outputFiles['ELF']
    return []

  def buildPythonServer(self, builder, sidlFile, language, generatedSource):
    baseName    = os.path.splitext(os.path.basename(sidlFile))[0]
    iorObjects  = self.buildPythonIOR(builder, sidlFile, language, generatedSource['Server IOR']['Cxx'])
    skelObjects = self.buildPythonSkeletons(builder, sidlFile, language, generatedSource['Server '+language]['Cxx'])
    config      = builder.pushConfiguration(language+' Server '+baseName)
    builder.link(iorObjects+skelObjects, os.path.join(os.getcwd(), 'lib'+baseName+'.so'), shared = 1)
    builder.popConfiguration()
    builder.saveConfiguration(language+' Server '+baseName)
    if 'Linked ELF' in config.outputFiles:
      return config.outputFiles['Linked ELF']
    return []

  def build(self, builder):
    for f in self.sidl:
      self.setupSIDL(builder, f)
      for language in self.clientLanguages:
        getattr(self, 'setup'+language+'Client')(builder, f, language)
      for language in self.serverLanguages:
        getattr(self, 'setup'+language+'Server')(builder, f, language)
      generatedSource = self.buildSIDL(builder, f)
      for language in self.clientLanguages:
        getattr(self, 'build'+language+'Client')(builder, f, language, generatedSource)
      for language in self.serverLanguages:
        getattr(self, 'build'+language+'Server')(builder, f, language, generatedSource)
    return
