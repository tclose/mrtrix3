def initParser(subparsers, base_parser):
  import argparse
  parser = subparsers.add_parser('msmt_5tt', parents=[base_parser], help='Derive MSMT CSD responses based on a co-registered 5TT image')
  arguments = parser.add_argument_group('Positional arguments specific to the \'msmt_5tt\' algorithm')
  arguments.add_argument('in_5tt', help='Input co-registered 5TT image')
  arguments.add_argument('out_gm', help='Output GM response text file')
  arguments.add_argument('out_wm', help='Output WM response text file')
  arguments.add_argument('out_csf', help='Output CSF response text file')
  options = parser.add_argument_group('Options specific to the \'msmt_5tt\' algorithm')
  options.add_argument('-dirs', help='Manually provide the fibre direction in each voxel (a tensor fit will be used otherwise)')
  options.add_argument('-fa', type=float, default=0.2, help='Upper fractional anisotropy threshold for isotropic tissue (i.e. GM and CSF) voxel selection')
  options.add_argument('-pvf', type=float, default=0.95, help='Partial volume fraction threshold for tissue voxel selection')
  options.add_argument('-wm_algo', metavar='algorithm', default='tournier', help='dwi2response algorithm to use for white matter single-fibre voxel selection')
  parser.set_defaults(algorithm='msmt_5tt')
  parser.set_defaults(single_shell=False)
  
  
  
def checkOutputFiles():
  import lib.app
  lib.app.checkOutputFile(lib.app.args.out_gm)
  lib.app.checkOutputFile(lib.app.args.out_wm)
  lib.app.checkOutputFile(lib.app.args.out_csf)



def getInputFiles():
  import os
  import lib.app
  from lib.runCommand import runCommand
  runCommand('mrconvert ' + lib.app.args.in_5tt + ' ' + os.path.join(lib.app.tempDir, '5tt.mif'))
  if lib.app.args.dirs:
    runCommand('mrconvert ' + lib.app.args.dirs + ' ' + os.path.join(lib.app.tempDir, 'dirs.mif') + ' -stride 0,0,0,1')



def execute():
  import math, os, shutil
  import lib.app
  from lib.getHeaderInfo import getHeaderInfo
  from lib.getImageStat  import getImageStat
  from lib.printMessage  import printMessage
  from lib.runCommand    import runCommand
  from lib.warnMessage   import warnMessage
  from lib.errorMessage  import errorMessage
  
  # Ideally want to use the oversampling-based regridding of the 5TT image from the SIFT model, not mrtransform
  # May need to commit 5ttregrid...

  # Verify input 5tt image
  sizes = [ int(x) for x in getHeaderInfo('5tt.mif', 'size').split() ]
  datatype = getHeaderInfo('5tt.mif', 'datatype')
  if not len(sizes) == 4 or not sizes[3] == 5 or not datatype.startswith('Float'):
    errorMessage('Imported anatomical image ' + os.path.basename(lib.app.args.in_5tt) + ' is not in the 5TT format')

  # Get shell information
  shells = [ float(x) for x in getHeaderInfo('dwi.mif', 'shells').split() ]
  if len(shells) < 3:
    warnMessage('Less than three b-value shells; response functions will not be applicable in MSMT CSD algorithm')

  # Get lmax information (if provided)
  wm_lmax = [ ]
  if lib.app.args.lmax:
    wm_lmax = [ int(x.strip()) for x in lib.app.args.lmax.split(',') ]
    if not len(wm_lmax) == len(shells):
      errorMessage('Number of manually-defined lmax\'s (' + str(len(wm_lmax)) + ') does not match number of b-value shells (' + str(len(shells)) + ')')
    for l in wm_lmax:
      if l%2:
        errorMessage('Values for lmax must be even')

  runCommand('dwi2tensor dwi.mif - -mask mask.mif | tensor2metric - -fa fa.mif -vector vector.mif')
  if not os.path.exists('dirs.mif'):
    shutil.copy('vector.mif', 'dirs.mif')
  runCommand('mrtransform 5tt.mif 5tt_regrid.mif -template fa.mif -interp linear')

  # Tissue masks
  runCommand('mrconvert 5tt_regrid.mif - -coord 3 0 -axes 0,1,2 | mrcalc - ' + str(lib.app.args.pvf) + ' -gt fa.mif ' + str(lib.app.args.fa) + ' -lt -mult mask.mif -mult gm_mask.mif')
  runCommand('mrconvert 5tt_regrid.mif - -coord 3 2 -axes 0,1,2 | mrcalc - ' + str(lib.app.args.pvf) + ' -gt mask.mif -mult wm_mask.mif')
  runCommand('mrconvert 5tt_regrid.mif - -coord 3 3 -axes 0,1,2 | mrcalc - ' + str(lib.app.args.pvf) + ' -gt fa.mif ' + str(lib.app.args.fa) + ' -lt -mult mask.mif -mult csf_mask.mif')

  # Revise WM mask to only include single-fibre voxels
  printMessage('Calling dwi2response recursively to select WM single-fibre voxels using \'' + lib.app.args.wm_algo + '\' algorithm')
  runCommand('dwi2response -quiet ' + lib.app.args.wm_algo + ' dwi.mif wm_ss_response.txt -mask wm_mask.mif -voxels wm_sf_mask.mif')

  # Check for empty masks
  gm_voxels  = int(getImageStat('gm_mask.mif',    'count', 'gm_mask.mif'))
  wm_voxels  = int(getImageStat('wm_sf_mask.mif', 'count', 'wm_sf_mask.mif'))
  csf_voxels = int(getImageStat('csf_mask.mif',   'count', 'csf_mask.mif'))
  empty_masks = [ ]
  if not gm_voxels:
    empty_masks.append('GM')
  if not wm_voxels:
    empty_masks.append('WM')
  if not csf_voxels:
    empty_masks.append('CSF')
  if empty_masks:
    message = ','.join(empty_masks)
    message += ' tissue mask'
    if len(empty_masks) > 1:
      message += 's'
    message += ' empty; cannot estimate response function'
    if len(empty_masks) > 1:
      message += 's'
    errorMessage(message)

  # For each of the three tissues, generate a multi-shell response
  # Since here we're guaranteeing that GM and CSF will be isotropic in all shells, let's use mrstats rather than sh2response (seems a bit weird passing a directions file to sh2response with lmax=0...)

  gm_responses  = [ ]
  wm_responses  = [ ]
  csf_responses = [ ]
  max_length = 0

  for index, b in enumerate(shells):
    int_b = str(int(round(b)))
    dwi_path = 'dwi_b' + int_b + '.mif'
    runCommand('dwiextract dwi.mif -shell ' + str(b) + ' ' + dwi_path)
    sizes = getHeaderInfo(dwi_path, 'size').split()
    if len(sizes) == 3:
      mean_path = dwi_path
    else:
      mean_path = 'dwi_b' + int_b + '_mean.mif'
      runCommand('mrmath ' + dwi_path + ' mean ' + mean_path + ' -axis 3')
    gm_mean  = float(getImageStat(mean_path, 'mean', 'gm_mask.mif'))
    csf_mean = float(getImageStat(mean_path, 'mean', 'csf_mask.mif'))
    gm_responses .append( str(gm_mean  * math.sqrt(4.0 * math.pi)) )
    csf_responses.append( str(csf_mean * math.sqrt(4.0 * math.pi)) )
    this_b_lmax_option = ''
    if wm_lmax:
      this_b_lmax_option = ' -lmax ' + str(wm_lmax[index])
    runCommand('amp2sh ' + dwi_path + ' - | sh2response - wm_sf_mask.mif dirs.mif wm_response_b' + int_b + '.txt' + this_b_lmax_option)
    wm_response = open('wm_response_b' + int_b + '.txt', 'r').read().split()
    wm_responses.append(wm_response)
    max_length = max(max_length, len(wm_response))

  with open('gm.txt', 'w') as f:
    for line in gm_responses:
      f.write(line + '\n')
  with open('wm.txt', 'w') as f:
    for line in wm_responses:
      line += ['0'] * (max_length - len(line))
      f.write(' '.join(line) + '\n')
  with open('csf.txt', 'w') as f:
    for line in csf_responses:
      f.write(line + '\n')

  shutil.copyfile('gm.txt',  os.path.join(lib.app.workingDir, lib.app.args.out_gm))
  shutil.copyfile('wm.txt',  os.path.join(lib.app.workingDir, lib.app.args.out_wm))
  shutil.copyfile('csf.txt', os.path.join(lib.app.workingDir, lib.app.args.out_csf))

  # Generate output 4D binary image with voxel selections
  runCommand('mrcat gm_mask.mif wm_sf_mask.mif csf_mask.mif voxels.mif -axis 3')

