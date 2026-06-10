{
 "deviceTypes": [
  {
   "name": "laser",
   "devices": [
    {
     "name": "laser",
     "isDisplay": true,
     "isEnabled": true,
     "deviceParams": [
      {
       "key": "basic",
       "type": "arrayParam",
       "arrayParam": {
        "params": [
         {
          "key": "x",
          "type": "double",
          "doubleValue": 0.0038068142025921015
         },
         {
          "key": "y",
          "type": "double",
          "doubleValue": 0.010024109909724466
         },
         {
          "key": "yaw",
          "type": "double",
          "doubleValue": -0.26697861548068147
         }
        ]
       }
      }
     ]
    },
    {
     "name": "laser1",
     "isDisplay": true,
     "isEnabled": true,
     "deviceParams": [
      {
       "key": "basic",
       "type": "arrayParam",
       "arrayParam": {
        "params": [
         {
          "key": "x",
          "type": "double",
          "doubleValue": -0.017812065958253465
         },
         {
          "key": "y",
          "type": "double",
          "doubleValue": -0.0043341410986055817
         },
         {
          "key": "yaw",
          "type": "double",
          "doubleValue": -0.44762967103008577
         }
        ]
       }
      }
     ]
    }
   ]
  },
  {
   "name": "motor",
   "devices": [
    {
     "name": "left",
     "isDisplay": true,
     "isEnabled": true,
     "deviceParams": [
      {
       "key": "basic",
       "type": "arrayParam",
       "arrayParam": {
        "params": [
         {
          "key": "y",
          "type": "double",
          "doubleValue": -0.0005146059846665163
         }
        ]
       }
      },
      {
       "key": "func",
       "type": "comboParam",
       "comboParam": {
        "childParams": [
         {
          "key": "walk",
          "params": [
           {
            "key": "wheelRadius",
            "type": "double",
            "doubleValue": -0.00042524985964820239
           }
          ]
         }
        ]
       }
      }
     ]
    },
    {
     "name": "right",
     "isDisplay": true,
     "isEnabled": true,
     "deviceParams": [
      {
       "key": "basic",
       "type": "arrayParam",
       "arrayParam": {
        "params": [
         {
          "key": "y",
          "type": "double",
          "doubleValue": 0.0005146059846665163
         }
        ]
       }
      },
      {
       "key": "func",
       "type": "comboParam",
       "comboParam": {
        "childParams": [
         {
          "key": "walk",
          "params": [
           {
            "key": "wheelRadius",
            "type": "double",
            "doubleValue": -4.3822994018796746e-05
           }
          ]
         }
        ]
       }
      }
     ]
    }
   ]
  },
  {
   "name": "controller",
   "devices": [
    {
     "name": "controller",
     "isDisplay": true,
     "isEnabled": true,
     "deviceParams": [
      {
       "key": "basic",
       "type": "arrayParam",
       "arrayParam": {
        "params": [
         {
          "key": "x",
          "type": "double",
          "doubleValue": -0.025901026027011505
         },
         {
          "key": "y",
          "type": "double",
          "doubleValue": -0.024419405583185411
         },
         {
          "key": "qw",
          "type": "double",
          "doubleValue": -0.094897766307168133
         },
         {
          "key": "qx",
          "type": "double",
          "doubleValue": 0.0031962716226250749
         },
         {
          "key": "qy",
          "type": "double",
          "doubleValue": -0.0049152554232140561
         },
         {
          "key": "qz",
          "type": "double",
          "doubleValue": 0.99546975748223965
         },
         {
          "key": "SSF",
          "type": "double",
          "doubleValue": 0.087191536574277251
         },
         {
          "key": "Bax",
          "type": "double",
          "doubleValue": -0.012364245864312123
         },
         {
          "key": "Bay",
          "type": "double",
          "doubleValue": 0.15073496846854378
         },
         {
          "key": "Baz",
          "type": "double",
          "doubleValue": -0.18028429242040978
         }
        ]
       }
      }
     ]
    }
   ]
  },
  {
   "name": "pgv",
   "devices": [
    {
     "name": "pgv",
     "isDisplay": true,
     "isEnabled": true,
     "deviceParams": [
      {
       "key": "basic",
       "type": "arrayParam",
       "arrayParam": {
        "params": [
         {
          "key": "x",
          "type": "double",
          "doubleValue": -0.0015378477862380137
         },
         {
          "key": "y",
          "type": "double",
          "doubleValue": 0.0032467647239253841
         },
         {
          "key": "yaw",
          "type": "double",
          "doubleValue": -0.22179454922505215
         }
        ]
       }
      }
     ]
    }
   ]
  },
  {
   "name": "camera",
   "devices": [
    {
     "name": "camera",
     "isDisplay": true,
     "isEnabled": true,
     "deviceParams": [
      {
       "key": "brand",
       "type": "comboParam",
       "comboParam": {
        "childParams": [
         {
          "key": "Orbbec-USB",
          "params": [
           {
            "key": "serialNumber",
            "type": "string",
            "stringValue": "CHBN842009S"
           }
          ]
         }
        ]
       }
      },
      {
       "key": "basic",
       "type": "arrayParam",
       "arrayParam": {}
      }
     ]
    }
   ]
  }
 ]
}
