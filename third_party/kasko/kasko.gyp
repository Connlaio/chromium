{
  'targets': [
    {
      # GN: //third_party/kasko:kasko_features
      'target_name': 'kasko_features',
      'type': 'none',
      'includes': [ '../../build/buildflag_header.gypi' ],
      'variables': {
        'buildflag_header_path': 'third_party/kasko/kasko_features.h',
        'buildflag_flags': [
          'ENABLE_KASKO=<(kasko)',
          'ENABLE_KASKO_HANG_REPORTS=<(kasko_hang_reports)',
          'ENABLE_KASKO_FAILED_RDV_REPORTS=<(kasko_failed_rdv_reports)',
        ],
      },
    },
  ],
  'conditions': [
    ['kasko==1', {
      'targets': [
        {
          # GN: //third_party/kasko:copy_kasko_dll
          'target_name': 'copy_kasko_dll',
          'type': 'none',
          'variables': {
            'kasko_exe_dir': '<(DEPTH)/third_party/kasko/binaries',
          },
          'outputs': [
            '<(PRODUCT_DIR)/kasko.dll',
            '<(PRODUCT_DIR)/kasko.dll.pdb',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(kasko_exe_dir)/kasko.dll',
                '<(kasko_exe_dir)/kasko.dll.pdb',
              ],
            },
          ],
        },
        {
          # GN: //third_party/kasko
          'target_name': 'kasko',
          'type': 'none',
          'dependencies': [
            'copy_kasko_dll',
            'kasko_features',
          ],
          'direct_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'kasko.dll.lib',
                ],
                'AdditionalLibraryDirectories': [
                  '../third_party/kasko/binaries'
                ],
              },
            },
            'include_dirs': [
              '../../third_party/kasko/binaries/include',
            ],
          },
          'export_dependent_settings': [
            'kasko_features',
          ]
        },
      ],
    }, {  # 'kasko==0'
      'targets': [
        {
          # GN: //third_party/kasko
          'target_name': 'kasko',
          'type': 'none',
          'dependencies': [
            'kasko_features',
          ],
        },
      ],
    }],
  ],  # 'conditions'
}
