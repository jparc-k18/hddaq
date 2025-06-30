import argparse
import logging
import logging.config
import os
import sys
import shutil
import time
import threading
import yaml
from rich.logging import RichHandler
from tkinter import *
from tkinter.scrolledtext import *

import MTMController
import MessageHandler
import MessageWindow
import CommentWindow
import StatusList
import DataSynchronizer

logger = logging.getLogger(__name__)

#______________________________________________________________________________
class Controller(Frame):
  #____________________________________________________________________________
  def __init__(self, path):
    Frame.__init__(self)
    self.path = path
    self.master.title('HDDAQ Controller (' + os.uname()[1] + ')')
    self.master.resizable(0,1)
    self.pack(fill=Y, expand=True)
    '''make widgets'''
    self.__make_menu()
    self.__make_labels()
    self.__make_comment_input()
    self.__make_buttons()
    self.__make_status_view()
    ''' check files'''
    self.check_files()
    '''control flags'''
    self.daq_state = -1
    self.daq_start_flag = 0
    self.daq_stop_flag = 0
    self.daq_auto_restart_flag = 0
    self.daq_auto_disk_switch_flag = 0
    self.last_ess_flag = 0
    self.master_controller_flag = 0
    self.is_switching = False
    '''time stamps'''
    self.comment_timestamp =  time.time()
    self.runno_timestamp =  time.time()
    self.maxevent_timestamp =  time.time()
    self.trig_timestamp =  time.time()
    self.starttime_timestamp =  time.time()
    self.status_timestamp = time.time()
    self.undertransition_timestamp =  time.time()
    self.prev_evnum = 0
    self.reader_check = False
    MTMController.set_host(args.mtm_host)
    MTMController.set_port(args.mtm_port)
  #____________________________________________________________________________
  def __make_menu(self):
    menubar= Menu(self)
    self.master.config(menu=menubar)
    self.menu1 = Menu(menubar, tearoff=0)
    menu2 = Menu(menubar, tearoff=0)
    menu3 = Menu(menubar, tearoff=0)
    menu4 = Menu(menubar, tearoff=0)
    menu5 = Menu(menubar, tearoff=0)
    menu6 = Menu(menubar, tearoff=0)
    menu10 = Menu(menubar, tearoff=0)
    menubar.add_cascade(label='Control',menu=self.menu1)
    menubar.add_cascade(label='Comment',menu=menu2)
    menubar.add_cascade(label='Message',menu=menu3)
    menubar.add_cascade(label='DAQ mode',menu=menu4)
    menubar.add_cascade(label='Options',menu=menu5)
    menubar.add_cascade(state=DISABLED,label=' ' * 62)
    menubar.add_cascade(label='Data Sync',menu=menu6)
    if len(secondary_path_list) == 0:
      menubar.entryconfig('Data Sync', state=DISABLED)
    menubar.add_cascade(label='Force control',menu=menu10)
    '''control'''
    self.menu1.add_command(label='Clean List', command=self.clean_list_command)
    self.menu1.add_separator()
    if len(primary_path_list) > 1:
      self.menu1.add_command(label='Switch Disk', command=self.switch_disk)
    else:
      self.menu1.add_command(label='Switch Disk', state=DISABLED)
    self.menu1.add_separator()
    self.menu1.add_command(label='Restart Frontend',
                           command=lambda:msgh.send_message('fe_end\0'))
    self.menu1.add_command(label='Quit', command=self.master.quit)
    '''force control'''
    menu10.add_command(label='Force Restart Frontend',
                      command=lambda:msgh.send_message('fe_exit\0'))
    menu10.add_separator()
    menu10.add_command(label='Force Start', command=self.start_command)
    menu10.add_command(label='Force Stop', command=self.stop_command)
    menu10.add_separator()
    menu10.add_command(label='Force Trig. ON',  command=self.trigon_command)
    menu10.add_command(label='Force Trig. OFF', command=self.trigoff_command)
    menu10.add_command(label='Force MTM reset', command=MTMController.mtm_reset)
    menu10.add_command(label='Force L2',        command=MTMController.force_L2)
    '''comment'''
    menu2.add_command(label='Write Comment', command=self.write_run_comment)
    menu2.add_command(label='Load Last Comment', command=self.load_last_comment)
    menu2.add_separator()
    menu2.add_command(label='Comment Window',
                      command=CommentWindow.comment_win.deiconify)
    '''window'''
    menu3.add_command(label='Message Window [ALL]',
                      command=msgw.all_msg_win.deiconify)
    menu3.add_command(label='Message Window [NORMAL]',
                      command=msgw.normal_msg_win.deiconify)
    menu3.add_command(label='Message Window [CONTROL]',
                      command=msgw.control_msg_win.deiconify)
    menu3.add_command(label='Message Window [STATUS]',
                      command=msgw.status_msg_win.deiconify)
    menu3.add_command(label='Message Window [WARNING]',
                      command=msgw.warning_msg_win.deiconify)
    menu3.add_command(label='Message Window [ERROR]',
                      command=msgw.error_msg_win.deiconify)
    menu3.add_command(label='Message Window [FATAL]',
                      command=msgw.fatal_msg_win.deiconify)
    '''daq mode'''
    menu4.add_command(label='Normal',
                      command=lambda:msgh.send_message('normal_mode\0'))
    menu4.add_command(label='Dummy',
                      command=lambda:msgh.send_message('dummy_mode\0'))
    '''options'''
    self.auto_trig_on = IntVar()
    self.auto_trig_on.set(0)
    self.auto_restart = IntVar()
    self.auto_restart.set(0)
    self.auto_disk_switch = IntVar()
    self.auto_disk_switch.set(0)
    self.auto_ess = IntVar()
    self.auto_ess.set(0)
    menu5.add_checkbutton(label='Auto Trig. ON',
                          onvalue=1, offvalue=0, variable=self.auto_trig_on)
    menu5.add_checkbutton(label='Auto Restart',
                          onvalue=1, offvalue=0, variable=self.auto_restart)
    if len(primary_path_list) > 1:
      menu5.add_checkbutton(label='Auto Disk Switch',
                            onvalue=1, offvalue=0,
                            variable=self.auto_disk_switch)
    else:
      menu5.add_checkbutton(label='Auto Disk Switch', state=DISABLED)
    if self.status_essdischarge() == -1:
      menu5.add_checkbutton(label='Auto ESS Trig', state=DISABLED)
    else:
      menu5.add_checkbutton(label='Auto ESS Trig',
                            onvalue=1, offvalue=0, variable=self.auto_ess)

    '''dsync'''
    menu6.add_command(label='Control Window',
                      command=dsync.msg_win.deiconify)
  #____________________________________________________________________________
  def __make_labels(self):
    '''DAQ label'''
    font1 = ('Helvetica', -24,'bold')
    self.label = Label(self, text='DAQ: Idle',
                       bg='black', fg='blue', font=font1)
    self.label.pack(side=TOP, fill=X)
    '''Last Run Start Time'''
    flabels = Frame(self)
    flabels.pack(side=TOP, fill=X, padx=100)
    self.lasttime = Label(flabels, text='Last Run Start Time:')
    self.lasttime.pack(side=LEFT, pady=10)
    self.disklink = Label(flabels, text='Data Storage Path:')
    self.disklink.pack(side=RIGHT, pady=10)
  #____________________________________________________________________________
  def __make_comment_input(self):
    fcomment = Frame(self)
    fcomment.pack(side=TOP)
    lcomment = Label(fcomment, text='Comment:')
    lcomment.pack(side=LEFT)
    self.comment = Entry(fcomment, width=86)
    self.comment.pack(side=LEFT)
  #____________________________________________________________________________
  def __make_buttons(self):
    fbuttons = Frame(self)
    fbuttons.pack(side=TOP)
    font1 = ('Helvetica', -24, 'bold')
    font2 = ('Helvetica', -16, 'normal')
    self.bstart = Button(fbuttons, text='Start', font=font1,
                         command=self.start_command)
    self.bstart.config(state=DISABLED)
    self.bstop = Button(fbuttons, text='Stop', font=font1,
                        command=self.stop_command)
    self.bstop.config(state=DISABLED)
    self.btrigon = Button(fbuttons, text='Trig. ON', font=font2,
                          command=self.trigon_command)
    self.btrigon.config(state=DISABLED)
    self.btrigoff = Button(fbuttons, text='Trig. OFF', font=font2,
                           command=self.trigoff_command)
    self.btrigoff.config(state=DISABLED)
    frunno = Frame(fbuttons)
    lrunno = Label(frunno, text='Run Number')
    lrunno.pack()
    self.runno_e = Entry(frunno, justify=RIGHT, width=12,
                         disabledbackground='white',
                         disabledforeground='black')
    self.runno_e.config(state=DISABLED)
    self.runno_e.pack()
    fmaxevent = Frame(fbuttons)
    lmaxevent = Label(fmaxevent, text='Maximum Events')
    lmaxevent.pack()
    self.maxevent_e = Entry(fmaxevent, justify=RIGHT, width=12,
                            disabledbackground='white',
                            disabledforeground='black')
    self.maxevent_e.insert(0,'10000000')
    self.maxevent_e.pack()
    fevent = Frame(fbuttons)
    levent = Label(fevent, text='Event Number')
    levent.pack()
    self.event_e = Entry(fevent, justify=RIGHT, width=12,
                         disabledbackground='white',
                         disabledforeground='black')
    self.event_e.insert(0,'0')
    self.event_e.config(state=DISABLED)
    self.event_e.pack()
    self.bstart.pack(side=LEFT, padx=5, pady=15)
    self.bstop.pack(side=LEFT, padx=5)
    self.btrigon.pack(side=LEFT, padx=5)
    self.btrigoff.pack(side=LEFT, padx=5)
    hspace1 = Label(fbuttons, width=1)
    hspace1.pack(side=LEFT)
    frunno.pack(side=LEFT, padx=12)
    fevent.pack(side=LEFT, padx=12)
    fmaxevent.pack(side=LEFT, padx=12)
  #____________________________________________________________________________
  def __make_status_view(self):
    flabels = Frame(self)
    flabels.pack(side=TOP, fill=X)
    lstatus = Label(flabels, anchor='w',
                    text=f'{"Nick Name":20} {"Node ID":9} {"Status":10} '
                    +f'{"Information"}')
    lstatus.pack(side=LEFT)
    lstatus.config(font=('Courier', -12, 'bold'))
    self.nodenum = Label(flabels, text='0 nodes/0 words')
    self.nodenum.pack(side=RIGHT)
    self.nodenum.config(font=('Courier', -12, 'bold'))
    fsttext = Frame(self)
    fsttext.pack(side=TOP, fill=BOTH, expand=True)
    self.sttext = ScrolledText(fsttext, font=('Courier', -12), height=10)
    self.sttext.config(state=DISABLED)
    self.sttext.tag_config('normal', foreground='black')
    self.sttext.tag_config('warning', foreground='yellow')
    self.sttext.tag_config('fatal', foreground='red')
    self.sttext.pack(side=LEFT, fill=BOTH, expand=True)
  #____________________________________________________________________________
  def check_files(self):
    self.message_dir = self.path+'/Messages'
    if os.path.isdir(self.message_dir) == False:
      os.mkdir(self.message_dir)
    misc_dir = self.path+'/misc'
    if os.path.isdir(misc_dir) == False:
      os.mkdir(misc_dir)
    self.comment_file = misc_dir+'/comment.txt'
    if os.path.isfile(self.comment_file) == False:
      with open(self.comment_file,'w') as f:
        f.write('')
    self.runno_file =  misc_dir+'/runno.txt'
    if os.path.isfile(self.runno_file) == False:
      self.set_runno(0)
    self.maxevent_file =  misc_dir+'/maxevent.txt'
    if os.path.isfile(self.maxevent_file) == False:
      self.set_maxevent(10000000)
    self.trig_file =  misc_dir+'/trig.txt'
    if os.path.isfile(self.trig_file) == False:
      self.set_trig_state('OFF')
    self.starttime_file =  misc_dir+'/starttime.txt'
    if os.path.isfile(self.starttime_file) == False:
      self.set_starttime('')
  #____________________________________________________________________________
  def start_command(self, mode='manual'):
    self.bstart.config(state=DISABLED)
    if self.auto_disk_switch.get() == 1:
      self.switch_disk()
    self.increment_runno()
    msgh.send_message('run ' + str(self.get_runno()) + '\0')
    '''for backword compatibility'''
    msgh.send_message('maxevent ' + str(999999999) + '\0')
    msgh.send_message('start\0')
    maxevent = self.maxevent_e.get()
    self.set_maxevent(maxevent)
    if mode=='auto': self.write_run_comment('START*')
    else           : self.write_run_comment('START ')
    self.set_starttime(time.strftime('%Y %m/%d %H:%M:%S'))
    MTMController.mtm_reset()
    self.daq_start_flag = 1
    self.master_controller_flag = 1
    self.menu1.entryconfig('Switch Disk', state=DISABLED)
    self.menu1.entryconfig('Restart Frontend', state=DISABLED)
    if self.master_controller_flag == 1:
      self.menu1.entryconfig('Quit', state=DISABLED)
  #____________________________________________________________________________
  def stop_command(self, mode='manual'):
    self.bstop.config(state=DISABLED)
    self.trigoff_command('auto')
    self.btrigon.config(state=DISABLED)
    self.daq_stop_flag = 1
    self.master.update()
    time.sleep(1)
    msgh.send_message('stop\0')
    if mode=='auto': self.write_run_comment('STOP* ')
    else           : self.write_run_comment('STOP  ')
    if len(primary_path_list) > 1:
      self.menu1.entryconfig('Switch Disk',
                             command=self.switch_disk, state=NORMAL)
    self.menu1.entryconfig('Restart Frontend',
                           command=lambda:msgh.send_message('fe_end\0'),
                           state=NORMAL)
    self.menu1.entryconfig('Quit',  command=self.master.quit, state=NORMAL)
  #____________________________________________________________________________
  def trigon_command(self, mode='manual'):
    self.btrigon.config(fg='black', bg='green', state=DISABLED)
    self.btrigoff.config(state=NORMAL)
    MTMController.trig_on()
    if mode=='auto': self.write_run_comment('G_ON* ')
    elif mode =='ess': self.write_run_comment('G_ON! ')
    else           : self.write_run_comment('G_ON  ')
    self.set_trig_state('ON')
  #____________________________________________________________________________
  def trigoff_command(self, mode='manual'):
    self.btrigon.config(fg='black', bg='#d9d9d9', state=NORMAL)
    self.btrigoff.config(state=DISABLED)
    MTMController.trig_off()
    if mode=='auto': self.write_run_comment('G_OFF*')
    elif mode =='ess': self.write_run_comment('G_OFF! ')
    else           : self.write_run_comment('G_OFF ')
    self.set_trig_state('OFF')
  #____________________________________________________________________________
  def status_essdischarge(self, not_found=-1):
    path_ess = "/mnt/raid/subdata/monitor_tmp/ESSdischarge.txt"
    try:
      with open(path_ess,'r') as f:
        state_ess = f.read().strip()
        if state_ess == "1": # ESS discharge
          return int(state_ess)
        elif state_ess == "0": # ESS stable
          return int(state_ess)
        else:
          print("no ESS status")
          state_ess = 2 # no value
          return int(state_ess)
    except FileNotFoundError:
      print("{path_ess} not found")
      self.auto_ess.set(0)
      return int(not_found)
    except IOError as e:
      print("ESS status check Error:{e}")
      self.auto_ess.set(0)
      return int(not_found)
  #____________________________________________________________________________
  def clean_list_command(self):
    status.cleanup_list()
    self.sttext.config(state=NORMAL)
    self.sttext.delete(1.0, END)
    self.sttext.config(state=DISABLED)
    msgh.send_message('anyone\0')
  #____________________________________________________________________________
  def write_run_comment(self, command='      '):
    comment = self.comment.get()
    if command=='      ' and comment=='': return
    runno = self.get_runno()
    CommentWindow.AddSaveComment(self.comment_file, runno,
                                 command+': '+comment)
    self.comment_timestamp = os.stat(self.comment_file).st_mtime
  #____________________________________________________________________________
  def load_last_comment(self):
    last = CommentWindow.GetLastComment()
    self.comment.delete(0, END)
    self.comment.insert(0, last)
  #____________________________________________________________________________
  def get_runno(self):
    with open(self.runno_file, 'r') as f:
      runno = f.readline()
    return int(runno)
  #____________________________________________________________________________
  def set_runno(self, runno):
    with open(self.runno_file, 'w') as f:
      f.write(str(runno))
  #____________________________________________________________________________
  def increment_runno(self):
    runno = self.get_runno()
    runno += 1
    self.set_runno(runno)
  #____________________________________________________________________________
  def get_maxevent(self):
    with open(self.maxevent_file, 'r') as f:
      maxevent = f.readline()
    return int(maxevent)
  #____________________________________________________________________________
  def set_maxevent(self, maxevent):
    with open(self.maxevent_file, 'w') as f:
      f.write(str(maxevent))
  #____________________________________________________________________________
  def get_trig_state(self):
    with open(self.trig_file, 'r') as f:
      state = f.readline()
    return state
  #____________________________________________________________________________
  def set_trig_state(self, state):
    with open(self.trig_file, 'w') as f:
      f.write(state)
  #____________________________________________________________________________
  def get_starttime(self):
    with open(self.starttime_file, 'r') as f:
      starttime = f.readline()
    return starttime
  #____________________________________________________________________________
  def set_starttime(self, starttime):
    with open(self.starttime_file, 'w') as f:
      f.write(starttime)
  #____________________________________________________________________________
  def beep_sound(self):
    os.system(sound_command)
  #____________________________________________________________________________
  def switch_disk(self):
    self.is_switching = True
    self.cur_path = os.path.realpath(args.data_path)
    self.new_path = self.cur_path
    for index, item in enumerate(primary_path_list):
      if self.cur_path == item:
        n = len(primary_path_list)
        self.new_path = primary_path_list[(index + 1) % n]
    logger.info(f'switch_disk : {self.cur_path} -> {self.new_path}')
    if self.new_path != self.cur_path:
      if not os.path.exists(self.new_path + '/misc'):
        os.makedirs(self.new_path + '/misc')
      if not os.path.exists(self.new_path + '/Messages'):
        os.makedirs(self.new_path + '/Messages')
      files = ['/misc/comment.txt',
               '/misc/runno.txt',
               '/misc/maxevent.txt',
               '/misc/trig.txt',
               '/misc/starttime.txt',
               '/recorder.log']
      for f in files:
        if os.path.isfile(self.cur_path + f):
          try:
            shutil.copy2(self.cur_path + f, self.new_path + f)
          except:
            logger.error(sys.exc_info())
    os.remove(args.data_path)
    os.symlink(self.new_path, args.data_path)
    self.is_switching = False
    self.update_starttime_window()
  #____________________________________________________________________________
  def update_starttime_window(self):
    stat = os.stat(self.starttime_file).st_mtime
    if self.starttime_timestamp != stat:
      self.starttime_timestamp = stat
      starttime = self.get_starttime()
      self.lasttime.configure(text='Last Run Start Time: ' + starttime)
      logger.info(f'update_startime_window : {starttime}')
    st = os.statvfs(os.path.realpath(args.data_path))
    free = st.f_frsize * st.f_bavail / 1000000000
    used = st.f_frsize * (st.f_blocks - st.f_bfree) / 1000000000
    total = st.f_frsize * st.f_blocks / 1000000000
    usage = float(used) * 100 / total
    info = (f'Data Storage Path: {os.path.realpath(args.data_path)}\n'
            +f'(Used: {used:.0f}/{total:.0f} GB  {usage:.1f}%)')
    if usage < 75.0:
      fg_color = 'black'
      bg_color = '#d9d9d9'
      flush = False
    elif usage < 90.0:
      fg_color = 'yellow'
      bg_color = 'black'
      flush = True
    else:
      fg_color = 'red'
      bg_color = 'black'
      flush = True
    bg = self.disklink.cget('bg')
    fg = self.disklink.cget('fg')
    if flush and bg == bg_color and fg == fg_color:
      bg_color = fg
      fg_color = bg
    self.disklink.configure(text=info, fg=fg_color, bg=bg_color)
  #____________________________________________________________________________
  def update_comment_window(self):
    stat = os.stat(self.comment_file).st_mtime
    if self.comment_timestamp != stat:
      self.comment_timestamp = stat
      with open(self.comment_file, 'r') as f:
        CommentWindow.ShowComment(f.read())
      self.load_last_comment()
  #____________________________________________________________________________
  def update_runno_window(self):
    stat = os.stat(self.runno_file).st_mtime
    if self.runno_timestamp != stat:
      self.runno_timestamp = stat
      runno = self.get_runno()
      logger.info(f'update_runno_window : {runno}')
      self.runno_e.config(state=NORMAL)
      self.runno_e.delete(0,END)
      self.runno_e.insert(0, str(runno))
      self.runno_e.config(state=DISABLED)
  #____________________________________________________________________________
  def update_maxevent_window(self):
    stat = os.stat(self.maxevent_file).st_mtime
    if self.maxevent_timestamp != stat:
      self.maxevent_timestamp = stat
      maxevent = self.get_maxevent()
      logger.info(f'update_maxevent_window : {maxevent}')
      self.maxevent_e.delete(0,END)
      self.maxevent_e.insert(0, str(maxevent))
  #____________________________________________________________________________
  def update_trig_status(self):
    stat = os.stat(self.trig_file).st_mtime
    if self.trig_timestamp != stat:
      self.trig_timestamp = stat
      state = self.get_trig_state()
      logger.info('update_trig_status : ' + state)
      if state == 'ON':
        self.btrigon.config(fg='black', bg='green', state=DISABLED)
        self.btrigoff.config(state=NORMAL)
      elif state == 'OFF':
        if self.daq_state == StatusList.S_RUNNING:
          self.btrigon.config(fg='black', bg='#d9d9d9', state=NORMAL)
        self.btrigoff.config(state=DISABLED)
      if self.daq_stop_flag == 1:
        self.btrigon.config(state=DISABLED)
  #____________________________________________________________________________
  def update_status_window(self):
    self.sttext.config(state=NORMAL)
    pos = self.sttext.yview()
    self.sttext.delete(1.0, END)
    n_nodes = 0
    n_readers = -1
    status.total_size = 0
    for item in status.statuslist:
      statusline = status.make_statusline(item)
      sttag = 'fatal' if item.status in ('NOUPDATE', 'DEAD') else 'normal'
      self.sttext.insert(END, statusline, sttag)
      if (item.src_id != StatusList.REC_ID and
          item.src_id != StatusList.DST_ID and
          item.src_id != StatusList.BLD_ID and
          sttag == 'normal'):
        n_nodes += 1
      if item.src_id == StatusList.BLD_ID:
        n_readers = item.n_readers
    self.sttext.config(state=DISABLED)
    self.sttext.yview_moveto(pos[0])
    # trigger_rate = ((int(status.dist_evnum) - self.prev_evnum)
    #                 / (time.time() - self.status_timestamp))
    self.nodenum.config(text=f'{n_nodes:3} nodes/{status.total_size:5} words')
    # self.prev_evnum = int(status.dist_evnum)
    # self.status_timestamp = time.time()
    if n_readers != n_nodes:
      self.reader_check = False
      logger.warning(f'node number mismatch : {n_nodes:3} nodes/{n_readers:5} readers')
      # self.bstart.config(state=DISABLED)
    else:
      if not self.reader_check:
        logger.info(f'node number ok : {n_nodes:3} nodes/{n_readers:5} readers')
      self.reader_check = True
      # if self.daq_state == StatusList.S_IDLE:
      #   self.bstart.config(state=NORMAL)

  #____________________________________________________________________________
  def update_evnum_window(self):
    if self.daq_state == StatusList.S_RUNNING :
      self.event_e.config(state=NORMAL)
      self.event_e.delete(0, END)
      self.event_e.insert(0, status.dist_evnum)
      self.event_e.config(state=DISABLED)
  #____________________________________________________________________________
  def update_global_state(self):
    gstate = status.global_state
    if gstate == self.daq_state:
      return
    else:
      self.daq_state = gstate
    logger.info(f'update_global_status : {self.daq_state}')
    if self.daq_state == StatusList.S_IDLE:
      self.label.config(text='DAQ: Idle', fg='blue', bg='black')
      self.bstart.config(state=NORMAL)
      self.bstop.config(state=DISABLED)
      self.btrigon.config(state=DISABLED)
      self.btrigoff.config(state=DISABLED)
      self.maxevent_e.config(state=NORMAL)
      self.daq_stop_flag = 0
      self.last_ess_flag = 0
      if len(primary_path_list) > 1:
        self.menu1.entryconfig('Switch Disk',
                               command=self.switch_disk, state=NORMAL)
      self.menu1.entryconfig('Restart Frontend',
                             command=lambda:msgh.send_message('fe_end\0'),
                             state=NORMAL)
      self.menu1.entryconfig('Quit',  command=self.master.quit, state=NORMAL)
      '''
      master flag is changed here for logging messages
      which come after stop command
      '''
      if self.master_controller_flag == 1:
        self.master_controller_flag = 0
    elif self.daq_state == StatusList.S_RUNNING:
      self.menu1.entryconfig('Switch Disk', state=DISABLED)
      self.menu1.entryconfig('Restart Frontend', state=DISABLED)
      if self.master_controller_flag == 1:
        self.menu1.entryconfig('Quit', state=DISABLED)
      if status.is_recorder == 1:
        if self.master_controller_flag == 1:
          self.label.config(text='DAQ: RUNNING [MASTER]',
                            fg='green', bg='black')
        else:
          self.label.config(text='DAQ: RUNNING [SLAVE]',
                            fg='green', bg='black')
      else:
        if self.master_controller_flag == 1:
          self.label.config(text='DAQ: DUMMY RUNNING [MASTER]',
                            fg='yellow', bg='black')
        else:
          self.label.config(text='DAQ: DUMMY RUNNING [SLAVE]',
                            fg='yellow', bg='black')
      self.bstart.config(state=DISABLED)
      self.bstop.config(state=NORMAL)
      self.maxevent_e.config(state=DISABLED)
      if self.daq_start_flag == 1:
        self.daq_start_flag = 0
        if self.auto_trig_on.get() == 1:
          self.trigon_command('auto')
        else:
          self.set_trig_state('OFF')
          self.btrigon.config(state=NORMAL)
      else:
        state = self.get_trig_state()
        if state == 'ON':
          self.btrigon.config(fg='black', bg='green', state=DISABLED)
          self.btrigoff.config(state=NORMAL)
        elif state == 'OFF':
          self.btrigon.config(fg='black', bg='#d9d9d9', state=NORMAL)
          self.btrigoff.config(state=DISABLED)
    else:
      self.label.config(text='DAQ: under Transition', fg='yellow', bg='red')
      self.bstart.config(state=DISABLED)
      self.bstop.config(state=DISABLED)
      self.btrigon.config(state=DISABLED)
      self.btrigoff.config(state=DISABLED)
      self.maxevent_e.config(state=DISABLED)
  #____________________________________________________________________________
  def under_transition_checker(self):
    if not self.daq_state in (StatusList.S_IDLE, StatusList.S_RUNNING):
      if self.master_controller_flag == 1:
        now = time.time()
        diff = now - self.undertransition_timestamp
        if diff > 4 :
          self.undertransition_timestamp = now
          thread = threading.Thread(target=self.beep_sound)
          thread.setDaemon(True)
          thread.start()
  #____________________________________________________________________________
  def updater(self):
    if self.is_switching:
      self.after(500, self.updater)
    '''Message'''
    linebuf = msgh.get_message()
    if self.master_controller_flag == 1:
      msgfile = (self.message_dir + '/msglog_run'
                 + str(self.get_runno()).zfill(5) + '.txt')
      msgw.AddSaveMessage(msgfile, linebuf)
    else:
      msgw.AddMessage(linebuf)
    '''Status'''
    status.update_list(linebuf)
    self.under_transition_checker()
    self.update_trig_status()
    self.update_evnum_window()
    self.update_maxevent_window()
    self.update_runno_window()
    self.update_comment_window()
    self.update_starttime_window()
    self.update_global_state()
    self.update_status_window()
    if status.check_null_nickname():
      msgh.send_message('anyone\0')
    '''
    Check event number stop and auto restart flag
    (only for the master controller that issued start command)
    '''
    if (self.daq_state == StatusList.S_RUNNING and
        self.master_controller_flag == 1):
      maxevent = int(self.maxevent_e.get())
      current  = int(status.dist_evnum)
      if current >= maxevent and maxevent != -1:
        self.stop_command('auto')
        if self.auto_restart.get()==1:
          self.daq_auto_restart_flag=1
    '''Auto restart'''
    if self.daq_state == StatusList.S_IDLE:
      if self.daq_auto_restart_flag==1:
        self.daq_auto_restart_flag=0
        self.start_command('auto')
    '''Auto Data Sync'''
    dsync.update()
    linebuf = dsync.get_message()
    if len(linebuf) > 0:
      dsync_log = (self.message_dir + '/dsync_run'
                   + str(self.get_runno()).zfill(5) + '.txt')
      dsync.AddSaveMessage(dsync_log, linebuf)
    else:
      dsync.AddMessage(linebuf)
    '''Auto ESS discharge Trigger '''
    if (self.daq_state == StatusList.S_RUNNING and
        self.auto_ess.get() == 1):
      if (self.status_essdischarge() == 1 and
          self.get_trig_state() == 'ON' and
          self.last_ess_flag == 0 ):
        self.trigoff_command('ess')
        self.last_ess_flag = 1
        print(f"ESS discharge Trig OFF ")
      if (self.status_essdischarge() in (0,2) and
          self.get_trig_state() == 'OFF' and
          self.last_ess_flag == 1 ):
        self.trigon_command('ess')
        self.last_ess_flag = 0
        print(f"ESS discharge recover Trig ON ")
      if (self.status_essdischarge() == -1 and
          self.last_ess_flag == -1 ):
        self.auto_ess.set(0)
        self.last_ess_flag = 0
        self.menu5.entryconfig('Auto ESS Trig', state=DISABLED)

    '''500ms repetition'''
    self.after(500, self.updater)

#______________________________________________________________________________
if __name__ == '__main__':
  '''
  parse argument
  '''
  parser = argparse.ArgumentParser()
  parser.add_argument('--cmsgd-host', default='localhost',
                      help='Which host name to connect CMSGD.')
  parser.add_argument('--cmsgd-port', type=int, default=8882,
                      help='Which port number to connect CMSGD.')
  parser.add_argument('--data-path', default='./data',
                      help='''
                      The data storage path.
                      If you switch disks, this must be a symbolic link.
                      ''')
  parser.add_argument('--data-path-list', default='./datapath.txt',
                      help='''
                      The text file which the list of
                      data storage path is written.
                      ''')
  parser.add_argument('--mtm-host', default='localhost',
                      help='Which host name to connect MTM controller')
  parser.add_argument('--mtm-port', type=int, default=24,
                      help='Which port number to connect MTM controller')
  args, unparsed = parser.parse_known_args()
  argc = len(sys.argv)
  logging.basicConfig(
    # level=logging.DEBUG,
    level=logging.INFO,
    format="%(message)s",
    # handlers=[RichHandler(show_time=False,
    #                       show_path=False,
    #                       rich_tracebacks=True)],
  )
  '''
  set storage path
  '''
  primary_path_list = []
  secondary_path_list = []
  if os.path.isfile(args.data_path_list):
    with open(args.data_path_list, 'r') as f:
      for line in f.readlines():
        line = line.split()
        if (len(line) == 0 or
            line[0][0] == '#' or
            not os.path.isdir(line[1])):
          continue
        if line[0][0].upper() == 'P':
          primary_path_list.append(line[1])
        if line[0][0].upper() == 'S':
          secondary_path_list.append(line[1])
  if not os.path.isdir(args.data_path):
    logger.error('no such directory : ' + args.data_path + '\n')
    parser.print_usage()
    quit()
  '''
  launch contoller, message handler and statuslist
  '''
  msgw = MessageWindow.MessageWindow()
  dsync = DataSynchronizer.DataSynchronizer(args.data_path,
                                            primary_path_list,
                                            secondary_path_list)
  app = Controller(args.data_path)
  msgh = MessageHandler.MessageHandler(args.cmsgd_host, args.cmsgd_port)
  status = StatusList.StatusList()
  '''
  play sound comman while under stansition state
  usage:  os.system(sound_command)
  '''
  sound_file = (os.path.abspath(os.path.dirname(__file__))
                + '/sound/under_transition.wav')
  # if 'eb0' in os.uname()[1]:
  #   sound_command = 'aplay ' + sound_file
  # else:
  #   sound_command = 'ssh eb0 aplay ' + sound_file
  # sound_command = 'sshpass -p beamtime ssh sks@k18epics aplay -Dhw:1,0 under_transition.wav'
  sound_command = 'ssh sks@k18epics aplay under_transition.wav'
  '''
  mainloop
  '''
  try:
    app.updater()
    app.mainloop()
  except:
    logger.info(sys.exc_info())
