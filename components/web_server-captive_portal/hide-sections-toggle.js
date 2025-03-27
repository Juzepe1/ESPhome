window.addEventListener('load', () => {
  const column = document.body
    .querySelector('esp-app')
    .shadowRoot.querySelector('#col_entities')
  const entities = column
    .querySelector('esp-entity-table')
    .shadowRoot.querySelector('div')

  let allowedGroups = ['Spectral Sensor Data']

  const hideStuff = (root) => {
    root.querySelectorAll('.tab-header').forEach((header) => {
      let display = 'none'
      if (allowedGroups.includes(header.textContent)) {
        display = ''
      }
      header.style.display = display
      header.nextSibling.style.display = display
    })
  }

  const run = () => {
    hideStuff(column)
    hideStuff(entities)
  }

  const observer = new MutationObserver(run)
  observer.observe(entities, { childList: true, subtree: false })
  run()

  const controls = document.createElement('div')
  controls.style.cssText = 'position:absolute;top:0;left:0;'

  const dash = document.createElement('button')
  dash.innerText = 'Dashboard'
  dash.addEventListener('click', () => {
    allowedGroups = ['Spectral Sensor Data']
    run()
  })
  controls.appendChild(dash)

  const settings = document.createElement('button')
  settings.innerText = 'Settings'
  settings.addEventListener('click', () => {
    allowedGroups = ['Spectral Sensor Settings', 'Device Settings']
    run()
  })
  controls.appendChild(settings)

  document.body.appendChild(controls)
})
