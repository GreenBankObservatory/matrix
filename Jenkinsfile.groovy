def lastCommit() {
    sh 'git log --format="%ae" | head -1 > commit-author.txt'                 
    lastAuthor = readFile('commit-author.txt').trim()
    sh 'rm commit-author.txt'
    return lastAuthor
}

// Used to send email notification of success or failure
def notify(status, details){
    def link = "http://colossus.gb.nrao.edu:8081/job/matrix/${env.BUILD_NUMBER}"
    def failure_description = "";
    def lastChangedBy = lastCommit()
    if (status == 'failure') {
        failure_description = """${env.BUILD_NUMBER} failed."""
    }
    emailext (
      to: "nsizemor@nrao.edu",
      subject: "${status}: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'",
      body: """${status}: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]':
        ${failure_description}
        Last commit by: ${lastChangedBy}
        Build summary: ${env.BUILD_URL}"""
    )
}

def checkout() {
    dir('matrix') { git branch: 'master', url: '/home/sandboxes/nsizemor/repos/matrix' }
}

def build() {
    sh '''
        echo TBD
    '''
}

def test() {
    sh '''
        echo TBD
    '''
}

def convertTestXMLOutput() {
    sh '''
        echo TBD
    '''
}


node {
    stage('Checkout') {
        try {
            cleanWs()
            checkout()
        } catch(error) {
            echo "Failure!"
            notify('failure', 'An error has occurred during the Checkout stage.')
            throw(error)
        }
    }
  
    stage('Build') {
        try {
            build()
        } catch(error) {
            echo "Failure!"
            notify('failure', 'An error has occurred during the Build stage.')
            throw(error)
        }
    }

    stage('Test') {
        try {
            test()
            // turns out junit can't read cppunit directly; need a bit of conversion
            //convertTestXMLOutput()
            //junit 'matrix/**/*.xml'
        } catch(error) {
            echo "Failure!"
            notify('failure', 'An error has occurred during the Test stage.')
            throw(error)
        }
    }

    stage('cppcheck') {
        try {
            sh '''
                ${WORKSPACE}/matrix/cppcheck.sh
            '''
            publishHTML([allowMissing: true, alwaysLinkToLastBuild: false, keepAll: false, reportDir: '', reportFiles: 'cppcheck.txt', reportName: 'cppcheck Report', reportTitles: ''])
        } catch(error) {
            echo "Failure!"
            notify('failure', 'An error has occurred during the cppcheck stage.')
            throw(error)
        }
    }

    stage('Documentation') {
        try {
            sh 'doxygen Doxyfile'
            publishHTML([allowMissing: true, alwaysLinkToLastBuild: false, keepAll: false, reportDir: '', reportFiles: 'codedocs/html/index.html', reportName: 'Doxygen', reportTitles: ''])
        } catch(error) {
            echo "Failure!"
            notify('failure', 'An error has occurred during the Documentation stage.')
            throw(error)
        }
    }
    
    stage('Notify') {
        notify('success', 'Matrix built successfully.')
    }
} 
